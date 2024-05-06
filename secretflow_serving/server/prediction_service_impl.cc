// Copyright 2023 Ant Group Co., Ltd.
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

#include "secretflow_serving/server/prediction_service_impl.h"

#include "brpc/closure_guard.h"
#include "brpc/controller.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/server/trace/trace.h"

namespace secretflow::serving {

PredictionServiceImpl::PredictionServiceImpl(const std::string& party_id)
    : party_id_(party_id),
      stats_({{"handler", "PredictionService"}, {"party_id", party_id_}}),
      init_flag_(false) {}

void PredictionServiceImpl::Init(
    const std::shared_ptr<PredictionCore>& prediction_core) {
  SERVING_ENFORCE(prediction_core, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(!init_flag_, errors::ErrorCode::LOGIC_ERROR);

  prediction_core_ = prediction_core;
  init_flag_ = true;
}

void PredictionServiceImpl::Predict(
    ::google::protobuf::RpcController* controller,
    const apis::PredictRequest* request, apis::PredictResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto* cntl = static_cast<brpc::Controller*>(controller);
  cntl->set_always_print_primitive_fields(true);

  auto span =
      CreateServerSpan(*cntl, request->header(), "PredictionService/Predict");
  auto scope = opentelemetry::trace::Tracer::WithActiveSpan(span);
  SpanAttrOption span_option;
  span_option.cntl = cntl;
  span_option.party_id = party_id_;
  span_option.service_id = request->service_spec().id();

  SPDLOG_DEBUG("predict begin, request: {}", request->ShortDebugString());
  yacl::ElapsedTimer timer;

  if (!init_flag_) {
    response->mutable_service_spec()->CopyFrom(request->service_spec());
    response->mutable_status()->set_code(errors::ErrorCode::NOT_READY);
    response->mutable_status()->set_msg(
        "prediction service is not ready to serve, please retry later.");
  } else {
    prediction_core_->Predict(request, response);
  }

  timer.Pause();
  SPDLOG_DEBUG("predict end, time: {}", timer.CountMs());

  span_option.code = response->status().code();
  span_option.msg = response->status().msg();
  SetSpanAttrs(span, span_option);

  RecordMetrics(*request, *response, timer.CountMs(), "Predict");
}

void PredictionServiceImpl::RecordMetrics(const apis::PredictRequest& request,
                                          const apis::PredictResponse& response,
                                          double duration_ms,
                                          const std::string& action) {
  stats_.api_request_duration_summary_family
      .Add(::prometheus::Labels({{"action", action},
                                 {"service_id", request.service_spec().id()}}),
           ::prometheus::Summary::Quantiles(
               {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))
      .Observe(duration_ms);
  stats_.api_request_counter_family
      .Add(::prometheus::Labels(
          {{"action", action},
           {"service_id", request.service_spec().id()},
           {"code", std::to_string(response.status().code())}}))
      .Increment();
  stats_.predict_counter.Increment(response.results().size());
}

PredictionServiceImpl::Stats::Stats(
    std::map<std::string, std::string> labels,
    const std::shared_ptr<::prometheus::Registry>& registry)
    : api_request_counter_family(
          ::prometheus::BuildCounter()
              .Name("prediction_request_count")
              .Help("How many prediction service api requests are handled by "
                    "this server.")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("prediction_request_duration_milliseconds")
              .Help("prediction service api request duration in milliseconds.")
              .Labels(labels)
              .Register(*registry)),
      predict_counter_family(
          ::prometheus::BuildCounter()
              .Name("prediction_sample_count")
              .Help("How many prediction samples are processed by "
                    "this services.")
              .Labels(labels)
              .Register(*registry)),
      predict_counter(predict_counter_family.Add(::prometheus::Labels{})) {}

}  // namespace secretflow::serving
