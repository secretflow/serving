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

#include "secretflow_serving/server/trace/trace.h"

namespace secretflow::serving {

ExecutionServiceImpl::ExecutionServiceImpl(
    const std::shared_ptr<ExecutionCore>& execution_core)
    : execution_core_(execution_core),
      stats_({{"handler", "ExecutionService"},
              {"party_id", execution_core->GetPartyID()}}) {}

void ExecutionServiceImpl::Execute(
    ::google::protobuf::RpcController* controller,
    const apis::ExecuteRequest* request, apis::ExecuteResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto* cntl = static_cast<brpc::Controller*>(controller);
  cntl->set_always_print_primitive_fields(true);

  auto span =
      CreateServerSpan(*cntl, request->header(), "ExecutionService/Execute");
  SpanAttrOption span_option;
  span_option.cntl = cntl;
  span_option.party_id = execution_core_->GetPartyID();
  span_option.service_id = request->service_spec().id();
  auto scope = opentelemetry::trace::Tracer::WithActiveSpan(span);

  yacl::ElapsedTimer timer;
  SPDLOG_DEBUG("execute begin, request: {}", request->ShortDebugString());
  execution_core_->Execute(request, response);
  timer.Pause();
  SPDLOG_DEBUG("execute end, response: {}", response->ShortDebugString());

  span_option.code = response->status().code();
  span_option.msg = response->status().msg();
  SetSpanAttrs(span, span_option);

  RecordMetrics(*request, *response, timer.CountMs(), "Execute");
}

void ExecutionServiceImpl::RecordMetrics(const apis::ExecuteRequest& request,
                                         const apis::ExecuteResponse& response,
                                         double duration_ms,
                                         const std::string& action) {
  stats_.api_request_counter_family
      .Add(::prometheus::Labels(
          {{"action", action},
           {"service_id", request.service_spec().id()},
           {"code", std::to_string(response.status().code())}}))
      .Increment();
  stats_.api_request_duration_summary_family
      .Add(::prometheus::Labels({{"action", action},
                                 {"service_id", request.service_spec().id()}}),
           ::prometheus::Summary::Quantiles(
               {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))
      .Observe(duration_ms);
}

ExecutionServiceImpl::Stats::Stats(
    std::map<std::string, std::string> labels,
    const std::shared_ptr<::prometheus::Registry>& registry)
    : api_request_counter_family(
          ::prometheus::BuildCounter()
              .Name("execution_request_count")
              .Help("How many execution requests are handled by "
                    "this server.")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("execution_request_duration_milliseconds")
              .Help("execution service api request duration in milliseconds")
              .Labels(labels)
              .Register(*registry)) {}

}  // namespace secretflow::serving
