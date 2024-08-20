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
#include "secretflow_serving/util/retry_policy.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

ExecuteContext::ExecuteContext(
    const apis::PredictRequest* request, apis::PredictResponse* response,
    const std::shared_ptr<Execution>& execution, const std::string& local_id,
    const std::unordered_map<std::string, apis::NodeIo>& node_io_map)
    : pred_req(request),
      pred_res(response),
      execution(execution),
      local_id(local_id) {
  exec_req.mutable_header()->CopyFrom(pred_req->header());
  exec_req.set_requester_id(local_id);
  exec_req.mutable_service_spec()->CopyFrom(pred_req->service_spec());

  if (node_io_map.empty()) {
    return;
  }
  auto* task = exec_req.mutable_task();
  task->set_execution_id(execution->id());
  auto entry_nodes = execution->GetEntryNodes();
  std::unordered_set<std::string> node_list;
  for (const auto& n : entry_nodes) {
    for (const auto& e : n->in_edges()) {
      node_list.emplace(e->src_node());
    }
  }
  for (const auto& n_name : node_list) {
    auto iter = node_io_map.find(n_name);
    SERVING_ENFORCE(iter != node_io_map.end(), errors::ErrorCode::LOGIC_ERROR,
                    "node:{} cannot be found in ctx(size:{})", n_name,
                    node_io_map.size());
    auto* node_io = task->add_nodes();
    // Do not `std::move` the node_io,
    // because other execution may also use it as an input.
    // TODO: Depending on the usage, let it can be moved by `std::move`.
    *node_io = iter->second;
  }
}

void ExecuteContext::SetFeatureSource(const std::string& target_id) {
  auto* feature_source = exec_req.mutable_feature_source();
  if (execution->IsGraphEntry()) {
    // entry execution need features
    // get target_id's feature param
    if (target_id == local_id && pred_req->predefined_features_size() != 0) {
      // only loacl execute will use `predefined_features`
      feature_source->set_type(apis::FeatureSourceType::FS_PREDEFINED);
      feature_source->mutable_predefineds()->CopyFrom(
          pred_req->predefined_features());
    } else {
      feature_source->set_type(apis::FeatureSourceType::FS_SERVICE);
      auto iter = pred_req->fs_params().find(target_id);
      SERVING_ENFORCE(iter != pred_req->fs_params().end(),
                      serving::errors::LOGIC_ERROR,
                      "missing {}'s feature params", target_id);
      feature_source->mutable_fs_param()->CopyFrom(iter->second);
    }
  } else {
    feature_source->set_type(apis::FeatureSourceType::FS_NONE);
  }
}

void ExecuteBase::GetOutputs(
    std::unordered_map<std::string, apis::NodeIo>* node_io_map) {
  auto* result = ctx_.exec_res.mutable_result();
  for (int i = 0; i < result->nodes_size(); ++i) {
    auto* result_node_io = result->mutable_nodes(i);
    auto prev_insert_iter = node_io_map->find(result_node_io->name());
    if (prev_insert_iter != node_io_map->end()) {
      // found node, merge ios
      auto& target_node_io = prev_insert_iter->second;
      SERVING_ENFORCE(target_node_io.ios_size() == result_node_io->ios_size(),
                      errors::ErrorCode::LOGIC_ERROR);
      for (int io_index = 0; io_index < target_node_io.ios_size(); ++io_index) {
        auto* target_io = target_node_io.mutable_ios(io_index);
        auto* io = result_node_io->mutable_ios(io_index);
        for (int data_index = 0; data_index < io->datas_size(); ++data_index) {
          target_io->add_datas(std::move(*(io->mutable_datas(data_index))));
        }
      }
    } else {
      auto node_name = result_node_io->name();
      node_io_map->emplace(node_name, std::move(*result_node_io));
    }
  }
}

RemoteExecute::RemoteExecute(ExecuteContext ctx, const std::string& target_id,
                             ::google::protobuf::RpcChannel* channel)
    : ExecuteBase{std::move(ctx)}, channel_(channel) {
  SetTarget(target_id);
  cntl_.set_max_retry(
      RetryPolicyFactory::GetInstance()->GetMaxRetryCount(target_id));
  span_option_.cntl = &cntl_;
  span_option_.is_client = true;
  span_option_.party_id = ctx.local_id;
  span_option_.service_id = ctx_.pred_req->service_spec().id();
}

RemoteExecute::~RemoteExecute() {
  if (executing_) {
    Cancel();
  }
}

void RemoteExecute::Run() {
  SERVING_ENFORCE(!executing_, errors::ErrorCode::LOGIC_ERROR);

  std::string service_info =
      fmt::format("ExecutionService/Execute: {}-{}", ctx_.local_id, target_id_);
  span_ =
      CreateClientSpan(&cntl_, service_info, ctx_.exec_req.mutable_header());
  // semisynchronous call
  apis::ExecutionService_Stub stub(channel_);
  stub.Execute(&cntl_, &ctx_.exec_req, &ctx_.exec_res, brpc::DoNothing());

  executing_ = true;
}

void RemoteExecute::Cancel() {
  if (!executing_) {
    return;
  }

  brpc::StartCancel(cntl_.call_id());

  executing_ = false;

  span_option_.code = errors::ErrorCode::UNEXPECTED_ERROR;
  span_option_.msg = "remote execute task is canceled.";
  SetSpanAttrs(span_, span_option_);
  span_->End();
}

void RemoteExecute::WaitToFinish() {
  if (!executing_) {
    return;
  }

  span_option_.code = errors::ErrorCode::OK;
  span_option_.msg = fmt::format("call ({}) from ({}) execute seccessfully",
                                 target_id_, ctx_.local_id);

  brpc::Join(cntl_.call_id());

  executing_ = false;

  if (cntl_.Failed()) {
    span_option_.msg =
        fmt::format("call ({}) from ({}) network error, msg:{}", target_id_,
                    ctx_.local_id, cntl_.ErrorText());
    span_option_.code = errors::ErrorCode::NETWORK_ERROR;
  } else if (!CheckStatusOk(ctx_.exec_res.status())) {
    span_option_.msg = fmt::format(
        fmt::format("call ({}) from ({}) execute failed: code({}), {}",
                    target_id_, ctx_.local_id, ctx_.exec_res.status().code(),
                    ctx_.exec_res.status().msg()));
    span_option_.code = errors::ErrorCode::NETWORK_ERROR;
  }
  SetSpanAttrs(span_, span_option_);
  span_->End();

  if (span_option_.code == errors::ErrorCode::OK) {
    MergeResponseHeader(ctx_.exec_res, ctx_.pred_res);
  } else {
    SERVING_THROW(span_option_.code, "{}", span_option_.msg);
  }
}

LocalExecute::LocalExecute(ExecuteContext ctx,
                           std::shared_ptr<ExecutionCore> execution_core)
    : ExecuteBase{std::move(ctx)}, execution_core_(std::move(execution_core)) {
  SetTarget(ctx_.local_id);
}

void LocalExecute::Run() {
  execution_core_->Execute(&ctx_.exec_req, &ctx_.exec_res);
  if (!CheckStatusOk(ctx_.exec_res.status())) {
    SERVING_THROW(ctx_.exec_res.status().code(), "{} exec failed: code({}), {}",
                  target_id_, ctx_.exec_res.status().code(),
                  ctx_.exec_res.status().msg());
  }
  MergeResponseHeader(ctx_.exec_res, ctx_.pred_res);
}

}  // namespace secretflow::serving
