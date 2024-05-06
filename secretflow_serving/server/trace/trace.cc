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
#include "opentelemetry/sdk/trace/simple_processor.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/trace/propagation/b3_propagator.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/provider.h"
#include "opentelemetry/trace/semantic_conventions.h"
#include "opentelemetry/trace/tracer_provider.h"

#include "secretflow_serving/server/trace/brpc_http_carrier.h"
#include "secretflow_serving/server/trace/bthreadlocal_context_storage.h"
#include "secretflow_serving/server/trace/spdlog_span_exporter.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/server/trace/span_info.pb.h"

namespace secretflow::serving {

namespace {

void AddBrpcInfoToSpan(
    const brpc::Controller& cntl,
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
    bool is_client) {
  span->SetAttribute(
      opentelemetry::trace::SemanticConventions::kHttpRequestMethod,
      std::string(brpc::HttpMethod2Str(cntl.http_request().method())));
  if (cntl.method()) {
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcMethod,
                       cntl.method()->full_name());
    span->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcService,
                       cntl.method()->service()->full_name());
  }
  span->SetAttribute(opentelemetry::trace::SemanticConventions::kRpcSystem,
                     "brpc");
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
    const TraceLogConfig& trace_log_config) {
  // use trace spdlog as exporter
  auto exporter = std::make_unique<SpdLogSpanExporter>(trace_log_config);
  return std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
      std::move(exporter));
}

void SetUpNoopTracerProvider() {
  auto trace_provider =
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
          new opentelemetry::trace::NoopTracerProvider());

  opentelemetry::trace::Provider::SetTracerProvider(trace_provider);
}

void SetUpBThreadStorage() {
  auto ctx_storage = opentelemetry::nostd::shared_ptr<
      opentelemetry::context::RuntimeContextStorage>(
      new BThreadLocalContextStorage());
  // set bthread local context storage as runtime ctx storage
  opentelemetry::context::RuntimeContext::SetRuntimeContextStorage(ctx_storage);
}

// TODO: provider should be protected by bthread mutex
void SetUpNormalTracerProvider(
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
        processors) {
  // Default is an always-on sampler.
  opentelemetry::sdk::resource::ResourceAttributes attributes = {
      {"service.name", "Secretflow Serving"},
      {"service.version", SERVING_VERSION_STRING}};
  auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);
  auto context = std::make_unique<opentelemetry::sdk::trace::TracerContext>(
      std::move(processors), resource);
  auto provider =
      opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
          new opentelemetry::sdk::trace::TracerProvider(std::move(context)));

  // Set the global trace provider
  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

void SetUpTracerProvider(
    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
        processors) {
  if (processors.empty()) {
    SetUpNoopTracerProvider();
    SPDLOG_INFO("no span processor configured, noop tracer will be used");
  } else {
    SetUpNormalTracerProvider(std::move(processors));
  }
}

void SetUpPropagator() {
  opentelemetry::context::propagation::GlobalTextMapPropagator::
      SetGlobalPropagator(
          opentelemetry::nostd::shared_ptr<
              opentelemetry::context::propagation::TextMapPropagator>(
              new opentelemetry::trace::propagation::HttpTraceContext()));
}

void InitTracer(const TraceConfig& trace_config) {
  std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>>
      processors;

  if (trace_config.trace_log_enable()) {
    SPDLOG_INFO("trace log span processor configured");

    auto processor = GetTraceLogProcessor(trace_config.trace_log_conf());

    processors.push_back(std::move(processor));
  }

  SetUpTracerProvider(std::move(processors));

  SetUpPropagator();

  SetUpBThreadStorage();
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    std::string_view tracer_name, std::string_view version) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  if (tracer_name.empty()) {
    return provider->GetTracer("secretflow_serving", SERVING_VERSION_STRING);
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

  std::string span_name = span_info;

  auto span = GetTracer(tracer_name)->StartSpan(span_name, options);

  return span;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info,
    apis::Header* request_header, std::string_view tracer_name) {
  return CreateClientSpan(
      cntl, service_info,
      request_header ? request_header->mutable_data() : nullptr, tracer_name);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info,
    ::google::protobuf::Map<std::string, std::string>* header_map,
    std::string_view tracer_name) {
  opentelemetry::trace::StartSpanOptions span_options;
  span_options.kind = trace_api::SpanKind::kClient;
  span_options.parent = opentelemetry::context::RuntimeContext::GetCurrent();
  std::string span_name = service_info;

  auto tracer = GetTracer(tracer_name);
  auto span = tracer->StartSpan(span_name, span_options);
  auto scope = tracer->WithActiveSpan(span);
  BrpcHttpCarrierSetable carrier(&cntl->http_request(), header_map);
  auto prop = opentelemetry::trace::propagation::B3PropagatorMultiHeader();
  auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  prop.Inject(carrier, current_ctx);

  return span;
}

void SetSpanAttrs(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span,
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
