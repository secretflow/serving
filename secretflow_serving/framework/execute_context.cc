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

#include "secretflow_serving/framework/execute_context.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

void ExecuteContext::CheckAndUpdateResponse() {
  CheckAndUpdateResponse(exec_res_);
}

void ExecuteContext::CheckAndUpdateResponse(
    const apis::ExecuteResponse& exec_res) {
  if (!CheckStatusOk(exec_res.status())) {
    SERVING_THROW(
        exec_res.status().code(),
        fmt::format("{} exec failed: code({}), {}", target_id_,
                    exec_res.status().code(), exec_res.status().msg()));
  }
  MergeResonseHeader(exec_res);
}

void ExecuteContext::MergeResonseHeader() { MergeResonseHeader(exec_res_); }

void ExecuteContext::MergeResonseHeader(const apis::ExecuteResponse& exec_res) {
  response_->mutable_header()->mutable_data()->insert(
      exec_res.header().data().begin(), exec_res.header().data().end());
}

void ExeResponseToIoMap(
    apis::ExecuteResponse& exec_res,
    std::unordered_map<std::string, std::shared_ptr<apis::NodeIo>>*
        node_io_map) {
  auto result = exec_res.mutable_result();
  for (int i = 0; i < result->nodes_size(); ++i) {
    auto result_node_io = result->mutable_nodes(i);
    auto prev_insert_iter = node_io_map->find(result_node_io->name());
    if (prev_insert_iter != node_io_map->end()) {
      // found node, merge ios
      auto& target_node_io = prev_insert_iter->second;
      SERVING_ENFORCE(target_node_io->ios_size() == result_node_io->ios_size(),
                      errors::ErrorCode::LOGIC_ERROR);
      for (int io_index = 0; io_index < target_node_io->ios_size();
           ++io_index) {
        auto target_io = target_node_io->mutable_ios(io_index);
        auto io = result_node_io->mutable_ios(io_index);
        for (int data_index = 0; data_index < io->datas_size(); ++data_index) {
          target_io->add_datas(std::move(*(io->mutable_datas(data_index))));
        }
      }
    } else {
      auto node_name = result_node_io->name();
      node_io_map->emplace(node_name, std::make_shared<apis::NodeIo>(
                                          std::move(*result_node_io)));
    }
  }
}

void ExecuteContext::GetResultNodeIo(
    std::unordered_map<std::string, std::shared_ptr<apis::NodeIo>>*
        node_io_map) {
  ExeResponseToIoMap(exec_res_, node_io_map);
}

void ExecuteContext::SetFeatureSource() {
  auto feature_source = exec_req_.mutable_feature_source();
  if (execution_->IsEntry()) {
    // entry execution need features
    // get target_id's feature param
    if (target_id_ == local_id_ && request_->predefined_features_size() != 0) {
      // only loacl execute will use `predefined_features`
      feature_source->set_type(apis::FeatureSourceType::FS_PREDEFINED);
      feature_source->mutable_predefineds()->CopyFrom(
          request_->predefined_features());
    } else {
      feature_source->set_type(apis::FeatureSourceType::FS_SERVICE);
      auto iter = request_->fs_params().find(target_id_);
      SERVING_ENFORCE(iter != request_->fs_params().end(),
                      serving::errors::LOGIC_ERROR,
                      "missing {}'s feature params", target_id_);
      feature_source->mutable_fs_param()->CopyFrom(iter->second);
    }
  } else {
    feature_source->set_type(apis::FeatureSourceType::FS_NONE);
  }
}

ExecuteContext::ExecuteContext(const apis::PredictRequest* request,
                               apis::PredictResponse* response,
                               const std::shared_ptr<Execution>& execution,
                               std::string target_id, std::string local_id)
    : request_(request),
      response_(response),
      local_id_(std::move(local_id)),
      target_id_(std::move(target_id)),
      execution_(execution) {
  exec_req_.mutable_header()->CopyFrom(request_->header());
  exec_req_.set_requester_id(local_id_);
  exec_req_.mutable_service_spec()->CopyFrom(request_->service_spec());

  SetFeatureSource();
}

void ExecuteContext::Execute(
    std::shared_ptr<::google::protobuf::RpcChannel> channel,
    brpc::Controller* cntl) {
  apis::ExecutionService_Stub stub(channel.get());
  stub.Execute(cntl, &exec_req_, &exec_res_, brpc::DoNothing());
}

void ExecuteContext::Execute(std::shared_ptr<ExecutionCore> execution_core) {
  execution_core->Execute(&exec_req_, &exec_res_);
}

void RemoteExecute::Run() {
  if (executing_) {
    SPDLOG_ERROR("Run should only be called once.");
    return;
  }

  std::string service_info =
      fmt::format("ExecutionService/Execute: {}-{}", exec_ctx_.LocalId(),
                  exec_ctx_.TargetId());
  span_ = CreateClientSpan(&cntl_, service_info,
                           exec_ctx_.ExecReq().mutable_header());

  // semisynchronous call
  exec_ctx_.Execute(channel_, &cntl_);

  executing_ = true;
}

}  // namespace secretflow::serving
