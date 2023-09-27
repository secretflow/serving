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

namespace secretflow::serving {

PredictionServiceImpl::PredictionServiceImpl(
    const std::shared_ptr<PredictionCore>& prediction_core)
    : prediction_core_(prediction_core),
      stats_({{"handler", "PredictionService"},
              {"service_id", prediction_core->GetServiceID()},
              {"party_id", prediction_core->GetPartyID()}}) {}

void PredictionServiceImpl::Predict(
    ::google::protobuf::RpcController* controller,
    const apis::PredictRequest* request, apis::PredictResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto* cntl = static_cast<brpc::Controller*>(controller);
  cntl->set_always_print_primitive_fields(true);

  SPDLOG_DEBUG("predict begin, request: {}", request->ShortDebugString());
  yacl::ElapsedTimer timer;
  prediction_core_->Predict(request, response);
  timer.Pause();
  SPDLOG_DEBUG("predict end, time: {}", timer.CountMs());

  RecordMetrics(*request, *response, timer.CountMs());
}

void PredictionServiceImpl::RecordMetrics(const apis::PredictRequest& request,
                                          const apis::PredictResponse& response,
                                          const double duration_ms) {
  stats_.api_request_duration_summary.Observe(duration_ms);
  stats_.api_request_counter_family
      .Add(::prometheus::Labels(
          {{"code", std::to_string(response.status().code())}}))
      .Increment();
  stats_.api_request_total_duration_family
      .Add(::prometheus::Labels(
          {{"code", std::to_string(response.status().code())}}))
      .Increment(duration_ms);
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
      api_request_total_duration_family(
          ::prometheus::BuildCounter()
              .Name("prediction_request_total_duration")
              .Help("total time to process the request in milliseconds")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("prediction_request_duration_seconds")
              .Help("prediction service api request duration in milliseconds.")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary(api_request_duration_summary_family.Add(
          ::prometheus::Labels{},
          ::prometheus::Summary::Quantiles(
              {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))),
      predict_counter_family(
          ::prometheus::BuildCounter()
              .Name("prediction_count")
              .Help("How many prediction samples are processed by "
                    "this server.")
              .Labels(labels)
              .Register(*registry)),
      predict_counter(predict_counter_family.Add(::prometheus::Labels{})) {}

}  // namespace secretflow::serving
