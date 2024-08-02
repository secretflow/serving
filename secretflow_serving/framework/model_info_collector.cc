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

#include "secretflow_serving/framework/model_info_collector.h"

#include <utility>

#include "spdlog/spdlog.h"

#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/apis/model_service.pb.h"

namespace secretflow::serving {

namespace {

using std::invoke_result_t;

class RetryRunner {
 public:
  RetryRunner(uint32_t retry_counts, uint32_t retry_interval_ms)
      : retry_counts_(retry_counts), retry_interval_ms_(retry_interval_ms) {}

  template <typename Func, typename... Args,
            typename = std::enable_if_t<
                std::is_same_v<bool, invoke_result_t<Func, Args...>>>>
  bool Run(Func&& f, Args&&... args) const {
    auto runner_func = [&] {
      return std::invoke(std::forward<Func>(f), std::forward<Args>(args)...);
    };
    for (uint32_t i = 0; i != retry_counts_; ++i) {
      if (!runner_func()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(retry_interval_ms_));
      } else {
        return true;
      }
    }
    return false;
  }

 private:
  uint32_t retry_counts_;
  uint32_t retry_interval_ms_;
};

}  // namespace

ModelInfoCollector::ModelInfoCollector(Options opts) : opts_(std::move(opts)) {
  // build model_info_
  model_info_.set_name(opts_.model_bundle->name());
  model_info_.set_desc(opts_.model_bundle->desc());
  auto* graph_view = model_info_.mutable_graph_view();
  graph_view->set_version(opts_.model_bundle->graph().version());
  for (const auto& node : opts_.model_bundle->graph().node_list()) {
    NodeView view;
    view.set_name(node.name());
    view.set_op(node.op());
    view.set_op_version(node.op_version());
    *(view.mutable_parents()) = node.parents();

    graph_view->mutable_node_list()->Add(std::move(view));
  }
  *(graph_view->mutable_execution_list()) =
      opts_.model_bundle->graph().execution_list();

  // build specific_party_map_
  auto execution_list = graph_view->execution_list();
  auto execution_list_size = execution_list.size();
  for (int i = 0; i != execution_list_size; ++i) {
    auto& execution = execution_list[i];
    if (execution.config().dispatch_type() == DispatchType::DP_SPECIFIED &&
        !execution.config().specific_flag()) {
      specific_party_map_[i] = std::string();
    }
  }

  // build local_node_views_
  const auto& node_list = graph_view->node_list();
  for (const auto& node_view : node_list) {
    local_node_views_[node_view.name()] = std::make_pair(
        node_view, std::set<std::string>(node_view.parents().begin(),
                                         node_view.parents().end()));
  }
}

void ModelInfoCollector::DoCollect() {
  RetryRunner runner(max_retry_cnt_, retry_interval_ms_);
  for (auto& [remote_party_id, channel] : *(opts_.remote_channel_map)) {
    SERVING_ENFORCE(runner.Run(&ModelInfoCollector::TryCollect, *this,
                               remote_party_id, channel),
                    serving::errors::LOGIC_ERROR,
                    "GetModelInfo from {} failed.", remote_party_id);
  }

  CheckAndSetSpecificMap();
}

bool ModelInfoCollector::TryCollect(
    const std::string& remote_party_id,
    const std::shared_ptr<::google::protobuf::RpcChannel>& channel) {
  brpc::Controller cntl;
  apis::GetModelInfoResponse response;
  apis::GetModelInfoRequest request;
  request.mutable_service_spec()->set_id(opts_.service_id);

  apis::ModelService_Stub stub(channel.get());
  stub.GetModelInfo(&cntl, &request, &response, nullptr);

  if (cntl.Failed()) {
    SPDLOG_WARN(
        "call ({}) from ({}) GetModelInfo failed, msg:{}, may need retry",
        remote_party_id, opts_.self_party_id, cntl.ErrorText());
    return false;
  }
  if (!CheckStatusOk(response.status())) {
    SPDLOG_WARN(
        "call ({}) from ({}) GetModelInfo failed, msg:{}, may need retry",
        remote_party_id, opts_.self_party_id, response.status().msg());
    return false;
  }
  model_info_map_[remote_party_id] = response.model_info();
  return true;
}

void ModelInfoCollector::CheckNodeViewList(
    const ::google::protobuf::RepeatedPtrField<::secretflow::serving::NodeView>&
        remote_node_views,
    const std::string& remote_party_id) {
  SERVING_ENFORCE_EQ(
      local_node_views_.size(), static_cast<size_t>(remote_node_views.size()),
      "node views size is not equal, {} : {}, {} : {}", opts_.self_party_id,
      local_node_views_.size(), remote_party_id, remote_node_views.size());
  for (const auto& remote_node_view : remote_node_views) {
    auto iter = local_node_views_.find(remote_node_view.name());
    SERVING_ENFORCE(iter != local_node_views_.end(),
                    serving::errors::LOGIC_ERROR,
                    "can't find node view {} from {}", remote_node_view.name(),
                    remote_party_id);
    auto& [local_node_view, local_node_parents] = iter->second;
    SERVING_ENFORCE_EQ(local_node_view.op(), remote_node_view.op(),
                       "node view {} op name is not equal, {} : {}, {} : {}",
                       remote_node_view.name(), opts_.self_party_id,
                       local_node_view.op(), remote_party_id,
                       remote_node_view.op());
    SERVING_ENFORCE_EQ(local_node_view.op_version(),
                       remote_node_view.op_version(),
                       "node view {} op version is not equal, {} : {}, {} : {}",
                       remote_node_view.name(), opts_.self_party_id,
                       local_node_view.op_version(), remote_party_id,
                       remote_node_view.op_version());
    auto remote_node_parents = std::set<std::string>(
        remote_node_view.parents().begin(), remote_node_view.parents().end());
    SERVING_ENFORCE(
        local_node_parents == remote_node_parents, serving::errors::LOGIC_ERROR,
        "node view {} op parents is not equal, {} : {}, {} : {}",
        remote_node_view.name(), opts_.self_party_id,
        fmt::join(local_node_parents.begin(), local_node_parents.end(), ","),
        remote_party_id,
        fmt::join(remote_node_parents.begin(), remote_node_parents.end(), ","));
  }
}

void ModelInfoCollector::CheckAndSetSpecificMap() {
  const auto& local_graph_view = model_info_.graph_view();

  for (auto& [remote_party_id, model_info] : model_info_map_) {
    SERVING_ENFORCE_EQ(model_info.name(), model_info_.name(),
                       "model name mismatch with {}: {}, local: {}: {}",
                       remote_party_id, model_info.name(), opts_.self_party_id,
                       model_info.name());

    const auto& graph_view = model_info.graph_view();
    SERVING_ENFORCE_EQ(graph_view.version(), local_graph_view.version(),
                       "version mismatch with {}: {}, local: {}: {}",
                       remote_party_id, graph_view.version(),
                       opts_.self_party_id, local_graph_view.version());
    SERVING_ENFORCE_EQ(
        local_graph_view.execution_list_size(),
        graph_view.execution_list_size(),
        "execution list size mismatch with {}: {}, local: {}: {}",
        remote_party_id, graph_view.execution_list_size(), opts_.self_party_id,
        local_graph_view.execution_list_size());

    CheckNodeViewList(graph_view.node_list(), remote_party_id);

    for (int i = 0; i != local_graph_view.execution_list_size(); ++i) {
      const auto& local_execution = local_graph_view.execution_list(i);
      const auto& remote_execution = graph_view.execution_list(i);

      SERVING_ENFORCE_EQ(remote_execution.nodes_size(),
                         local_execution.nodes_size(),
                         "node count mismatch: {}: {}, local: {}: {}",
                         remote_party_id, remote_execution.nodes_size(),
                         opts_.self_party_id, local_execution.nodes_size());

      SERVING_ENFORCE(
          remote_execution.config().dispatch_type() ==
              local_execution.config().dispatch_type(),
          serving::errors::LOGIC_ERROR,
          "node count mismatch: {}: {}, local: {}: {}", remote_party_id,
          DispatchType_Name(remote_execution.config().dispatch_type()),
          opts_.self_party_id,
          DispatchType_Name(local_execution.config().dispatch_type()));

      for (int j = 0; j != local_execution.nodes_size(); ++j) {
        SERVING_ENFORCE(remote_execution.nodes(j) == local_execution.nodes(j),
                        serving::errors::LOGIC_ERROR,
                        "node name mismatch: {}: {}, local: {}: {}",
                        remote_party_id, remote_execution.nodes(j),
                        opts_.self_party_id, local_execution.nodes(j));
      }

      if (remote_execution.config().dispatch_type() ==
          DispatchType::DP_SPECIFIED) {
        if (remote_execution.config().specific_flag()) {
          SERVING_ENFORCE(specific_party_map_[i].empty(),
                          serving::errors::LOGIC_ERROR,
                          "{} execution specific to multiple parties", i);
          specific_party_map_[i] = remote_party_id;
        }
      }
    }
  }

  for (auto& [id, party_id] : specific_party_map_) {
    SERVING_ENFORCE(!party_id.empty(), serving::errors::LOGIC_ERROR,
                    "{} execution specific to no party", id);
  }
}

}  // namespace secretflow::serving
