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

#include "secretflow_serving/server/model_service_impl.h"

#include "brpc/closure_guard.h"
#include "brpc/controller.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

ModelServiceImpl::ModelServiceImpl(std::map<std::string, ModelInfo> model_infos,
                                   const std::string& self_party_id)
    : model_infos_(std::move(model_infos)),
      self_party_id_(self_party_id),
      stats_({{"handler", "ModelService"}, {"party_id", self_party_id_}}) {}

void ModelServiceImpl::GetModelInfo(
    ::google::protobuf::RpcController* controller,
    const apis::GetModelInfoRequest* request,
    apis::GetModelInfoResponse* response, ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto* cntl = static_cast<brpc::Controller*>(controller);
  cntl->set_always_print_primitive_fields(true);

  yacl::ElapsedTimer timer;

  response->mutable_service_spec()->CopyFrom(request->service_spec());

  auto it = model_infos_.find(request->service_spec().id());
  if (it == model_infos_.end()) {
    response->mutable_status()->set_code(errors::ErrorCode::NOT_FOUND);
    response->mutable_status()->set_msg(fmt::format(
        "invalid service spec id: {}", request->service_spec().id()));
  } else {
    *(response->mutable_model_info()) = it->second;
    response->mutable_status()->set_code(errors::ErrorCode::OK);
  }

  timer.Pause();
  RecordMetrics(*request, *response, timer.CountMs(), "GetModelInfo");
}

void ModelServiceImpl::RecordMetrics(const apis::GetModelInfoRequest& request,
                                     const apis::GetModelInfoResponse& response,
                                     double duration_ms,
                                     const std::string& action) {
  stats_.api_request_duration_summary_family
      .Add(::prometheus::Labels({{"service_id", request.service_spec().id()},
                                 {"action", action}}),
           ::prometheus::Summary::Quantiles(
               {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))
      .Observe(duration_ms);
  stats_.api_request_counter_family
      .Add(::prometheus::Labels(
          {{"service_id", request.service_spec().id()},
           {"code", std::to_string(response.status().code())},
           {"action", action}}))
      .Increment();
}

ModelServiceImpl::Stats::Stats(
    std::map<std::string, std::string> labels,
    const std::shared_ptr<::prometheus::Registry>& registry)
    : api_request_counter_family(
          ::prometheus::BuildCounter()
              .Name("model_service_request_count")
              .Help("How many model service api requests are handled by "
                    "this server.")
              .Labels(labels)
              .Register(*registry)),
      api_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("model_service_request_duration_seconds")
              .Help("model service api request duration in "
                    "milliseconds.")
              .Labels(labels)
              .Register(*registry)) {}

}  // namespace secretflow::serving
