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

#include "secretflow_serving/server/execution_service_impl.h"

#include "brpc/closure_guard.h"
#include "brpc/controller.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"
namespace secretflow::serving {

ExecutionServiceImpl::ExecutionServiceImpl(
    const std::shared_ptr<ExecutionCore>& execution_core)
    : execution_core_(execution_core),
      stats_({{"handler", "ExecutionService"},
              {"service_id", execution_core->GetServiceID()},
              {"party_id", execution_core->GetPartyID()}}) {}

void ExecutionServiceImpl::Execute(
    ::google::protobuf::RpcController* controller,
    const apis::ExecuteRequest* request, apis::ExecuteResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller* cntl = static_cast<brpc::Controller*>(controller);
  cntl->set_always_print_primitive_fields(true);

  yacl::ElapsedTimer timer;
  SPDLOG_DEBUG("execute begin, request: {}", request->ShortDebugString());
  execution_core_->Execute(request, response);
  timer.Pause();
  SPDLOG_DEBUG("execute end, response: {}", response->ShortDebugString());

  RecordMetrics(*request, *response, timer.CountMs());
}

void ExecutionServiceImpl::RecordMetrics(const apis::ExecuteRequest& request,
                                         const apis::ExecuteResponse& response,
                                         double duration_ms) {
  stats_.api_request_counter_family
      .Add(::prometheus::Labels(
          {{"code", std::to_string(response.status().code())}}))
      .Increment();
  stats_.api_request_totol_duration_family
      .Add(::prometheus::Labels(
          {{"code", std::to_string(response.status().code())}}))
      .Increment(duration_ms);
  stats_.api_request_duration_summary.Observe(duration_ms);
}

ExecutionServiceImpl::Stats::Stats(
    std::map<std::string, std::string> labels,
    const std::shared_ptr<::prometheus::Registry>& registry)
    : api_request_counter_family(
          ::prometheus::BuildCounter()
              .Name("execution_request_count_family")
              .Help("How many execution requests are handled by "
                    "this server.")
              .Labels(labels)
              .Register(*registry)),
      api_request_totol_duration_family(
          ::prometheus::BuildCounter()
              .Name("execution_request_total_duration_family")
              .Help("total time to process the request in milliseconds")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("execution_request_duration_family")
              .Help("prediction service api request duration in milliseconds")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary(api_request_duration_summary_family.Add(
          ::prometheus::Labels(),
          ::prometheus::Summary::Quantiles(
              {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))) {}

}  // namespace secretflow::serving
