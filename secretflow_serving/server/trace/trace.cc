// Copyright 2024 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "secretflow_serving/server/trace/trace.h"

#include "opentelemetry/context/propagation/global_propagator.h"
#include "opentelemetry/sdk/trace/batch_span_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/propagation/b3_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/tracer_provider.h"

#include "secretflow_serving/server/trace/brpc_http_carrier.h"
#include "secretflow_serving/server/trace/bthreadlocal_context_storage.h"
#include "secretflow_serving/server/trace/noop.h"
#include "secretflow_serving/server/trace/spdlog_span_exporter.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/server/trace/span_info.pb.h"

namespace secretflow::serving {

DEFINE_uint32(trace_processor_max_queue_size, 10240,
              "Flag for "
              "`opentelemetry::sdk::trace::BatchSpanProcessorOptions.max_queue_"
              "size`. The maximum "
              "buffer/queue size. After the size is reached, spans "
              "are dropped");

DEFINE_uint32(
    trace_processor_schedule_delay_millis, 2000,
    "Flag for "
    "`opentelemetry::sdk::trace::BatchSpanProcessorOptions.schedule_"
    "delay_millis`. The time interval between two consecutive exports.");

DEFINE_uint32(
    trace_processor_max_export_batch_size, 512,
    "Flag for "
    "`opentelemetry::sdk::trace::BatchSpanProcessorOptions.max_export_"
    "batch_size`. The maximum batch size of every export. It must be "
    "smaller or equal to max_queue_size.");

namespace {

void AddBrpcInfoToSpan(
    const brpc::Controller& cntl,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span,
    bool is_client) {
  span->SetAttribute(
      opentelemetry::trace::SemanticConventions::kHttpRequestMethod,
      std::string(brpc::HttpMethod2Str(cntl.http_request().method())));
  if (cntl.method() != nullptr) {
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcMethod,
                       cntl.method()->full_name());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcService,
                       cntl.method()->service()->full_name());
  }
  if (is_client) {
    span->SetAttribute(
        opentelemetry::trace::SemanticConventions::kServerAddress,
        butil::endpoint2str(cntl.remote_side()).c_str());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kServerPort,
                       cntl.remote_side().port);
    span->SetAttribute(
        opentelemetry::trace::SemanticConventions::kSourceAddress,
        butil::endpoint2str(cntl.local_side()).c_str());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kSourcePort,
                       cntl.local_side().port);
  } else {
    span->SetAttribute(
        opentelemetry::trace::SemanticConventions::kServerAddress,
        butil::endpoint2str(cntl.local_side()).c_str());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kServerPort,
                       cntl.local_side().port);
    span->SetAttribute(
        opentelemetry::trace::SemanticConventions::kSourceAddress,
        butil::endpoint2str(cntl.remote_side()).c_str());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kSourcePort,
                       cntl.remote_side().port);
  }

  std::ostringstream os;
  cntl.http_request().uri().Print(os);
  span->SetAttribute(opentelemetry::trace::SemanticConventions::kUrlFull,
                     os.str());
  span->SetAttribute(opentelemetry::trace::SemanticConventions::kUrlPath,
                     cntl.http_request().uri().path());
  span->SetAttribute("request_protocol",
                     brpc::ProtocolTypeToString(cntl.request_protocol()));
}
}  // namespace

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> GetTraceLogProcessor(
    const TraceConfig& trace_config) {
  if (!trace_config.trace_log_enable()) {
    return std::make_unique<trace::NoopSpanProcessor>();
  } else {
    // use trace spdlog as exporter
    auto exporter =
        std::make_unique<SpdLogSpanExporter>(trace_config.trace_log_conf());
    opentelemetry::sdk::trace::BatchSpanProcessorOptions options{
        .max_queue_size = FLAGS_trace_processor_max_export_batch_size,
        .schedule_delay_millis = std::chrono::milliseconds(
            FLAGS_trace_processor_schedule_delay_millis),
        .max_export_batch_size = FLAGS_trace_processor_max_export_batch_size,
    };
    return opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
        std::move(exporter), options);
  }
}

void SetUpBThreadStorage() {
  auto ctx_storage = opentelemetry::nostd::shared_ptr<
      opentelemetry::context::RuntimeContextStorage>(
      new BThreadLocalContextStorage());
  // set bthread local context storage as runtime ctx storage
  opentelemetry::context::RuntimeContext::SetRuntimeContextStorage(ctx_storage);
}

void SetUpTracerProvider(
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
        processors,
    bool enable_log) {
  opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>
      provider;
  if (enable_log) {
    // Default is an always-on sampler.
    opentelemetry::sdk::resource::ResourceAttributes attributes = {
        {"service.name", "Secretflow Serving"},
        {"service.version", SERVING_VERSION_STRING},
        {opentelemetry::trace::SemanticConventions::kRpcSystem, "brpc"}};
    auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);
    auto context = std::make_unique<opentelemetry::sdk::trace::TracerContext>(
        std::move(processors), resource);
    provider =
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
            new opentelemetry::sdk::trace::TracerProvider(std::move(context)));
  } else {
    provider =
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
            new opentelemetry::sdk::trace::TracerProvider(
                std::move(processors)));
    SPDLOG_INFO("simple trace provider for disable trace export");
  }
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

void SetUpPropagator() {
  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

void InitTracer(const TraceConfig& trace_config) {
  auto processor = GetTraceLogProcessor(trace_config);
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
      processors;
  processors.emplace_back(std::move(processor));
  SPDLOG_INFO("trace span processor configured");

  SetUpTracerProvider(std::move(processors), trace_config.trace_log_enable());

  SetUpPropagator();

  SetUpBThreadStorage();
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    std::string_view tracer_name, std::string_view version) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  if (tracer_name.empty()) {
    return provider->GetTracer("services");
  }
  return provider->GetTracer(tracer_name, version);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateServerSpan(
    const brpc::Controller& cntl, const apis::Header& request_header,
    const std::string& span_info, std::string_view tracer_name) {
  BrpcHttpCarrierGetable carrier(cntl.http_request(), request_header.data());
  auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  auto prop = opentelemetry::trace::propagation::B3PropagatorMultiHeader();
  auto context = prop.Extract(carrier, current_ctx);

  opentelemetry::trace::StartSpanOptions options;
  options.kind = opentelemetry::trace::SpanKind::kServer;  // server
  options.parent = opentelemetry::trace::GetSpan(context)->GetContext();

  const std::string& span_name = span_info;

  auto span = GetTracer(tracer_name)->StartSpan(span_name, options);

  return span;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info,
    apis::Header* request_header, std::string_view tracer_name) {
  return CreateClientSpan(
      cntl, service_info,
      (request_header != nullptr) ? request_header->mutable_data() : nullptr,
      tracer_name);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info,
    ::google::protobuf::Map<std::string, std::string>* header_map,
    std::string_view tracer_name) {
  opentelemetry::trace::StartSpanOptions span_options;
  span_options.kind = trace_api::SpanKind::kClient;
  span_options.parent = opentelemetry::context::RuntimeContext::GetCurrent();
  const std::string& span_name = service_info;

  auto tracer = GetTracer(tracer_name);
  auto span = tracer->StartSpan(span_name, span_options);
  auto scope = opentelemetry::trace::Tracer::WithActiveSpan(span);
  BrpcHttpCarrierSetable carrier(&cntl->http_request(), header_map);
  auto prop = opentelemetry::trace::propagation::B3PropagatorMultiHeader();
  auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  prop.Inject(carrier, current_ctx);

  return span;
}

void SetSpanAttrs(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span,
    const SpanAttrOption& option) {
  SpanInfo span_info;
  span_info.set_model_service_id(option.service_id);
  span_info.set_party_id(option.party_id);

  span->SetAttribute("span_info", PbToJson(&span_info));
  span->SetStatus(option.code == errors::ErrorCode::OK
                      ? opentelemetry::trace::StatusCode::kOk
                      : opentelemetry::trace::StatusCode::kError,
                  fmt::format("code: {}, msg: {}", option.code, option.msg));
  if (option.cntl != nullptr) {
    AddBrpcInfoToSpan(*option.cntl, span, option.is_client);
  }
}

}  // namespace secretflow::serving
