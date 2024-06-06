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

#include "secretflow_serving/framework/model_info_processor.h"

#include <set>
#include <utility>

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/apis/model_service.pb.h"

namespace secretflow::serving {

ModelInfoProcessor::ModelInfoProcessor(
    std::string local_party_id, const ModelInfo& local_model_info,
    const std::unordered_map<std::string, ModelInfo>& remote_model_info)
    : local_party_id_(local_party_id),
      local_model_info_(&local_model_info),
      remote_model_info_(&remote_model_info) {
  // build local_node_views_
  const auto& node_list = local_model_info_->graph_view().node_list();
  for (const auto& node_view : node_list) {
    local_node_views_[node_view.name()] = std::make_pair(
        node_view, std::set<std::string>(node_view.parents().begin(),
                                         node_view.parents().end()));
  }

  CheckAndSetSpecificMap();
}

std::unordered_map<size_t, std::string> ModelInfoProcessor::GetSpecificMap() {
  return specific_map_;
}

void ModelInfoProcessor::CheckAndSetSpecificMap() {
  // init specific_map
  auto execution_list = local_model_info_->graph_view().execution_list();
  auto execution_list_size = execution_list.size();
  for (int i = 0; i != execution_list_size; ++i) {
    auto& execution = execution_list[i];
    if (execution.config().dispatch_type() == DispatchType::DP_SPECIFIED &&
        !execution.config().specific_flag()) {
      specific_map_[i] = std::string();
    }
  }

  const auto& local_graph_view = local_model_info_->graph_view();

  for (auto& [remote_party_id, model_info] : *remote_model_info_) {
    SERVING_ENFORCE_EQ(model_info.name(), local_model_info_->name(),
                       "model name mismatch with {}: {}, local: {}: {}",
                       remote_party_id, model_info.name(), local_party_id_,
                       local_model_info_->name());

    const auto& graph_view = model_info.graph_view();
    SERVING_ENFORCE_EQ(graph_view.version(), local_graph_view.version(),
                       "version mismatch with {}: {}, local: {}: {}",
                       remote_party_id, graph_view.version(), local_party_id_,
                       local_graph_view.version());
    SERVING_ENFORCE_EQ(
        local_graph_view.execution_list_size(),
        graph_view.execution_list_size(),
        "execution list size mismatch with {}: {}, local: {}: {}",
        remote_party_id, graph_view.execution_list_size(), local_party_id_,
        local_graph_view.execution_list_size());

    CheckNodeViewList(graph_view.node_list(), remote_party_id);

    for (int i = 0; i != local_graph_view.execution_list_size(); ++i) {
      const auto& local_execution = local_graph_view.execution_list(i);
      const auto& remote_execution = graph_view.execution_list(i);

      SERVING_ENFORCE_EQ(remote_execution.nodes_size(),
                         local_execution.nodes_size(),
                         "node count mismatch: {}: {}, local: {}: {}",
                         remote_party_id, remote_execution.nodes_size(),
                         local_party_id_, local_execution.nodes_size());

      SERVING_ENFORCE(
          remote_execution.config().dispatch_type() ==
              local_execution.config().dispatch_type(),
          serving::errors::LOGIC_ERROR,
          "node count mismatch: {}: {}, local: {}: {}", remote_party_id,
          DispatchType_Name(remote_execution.config().dispatch_type()),
          local_party_id_,
          DispatchType_Name(local_execution.config().dispatch_type()));

      for (int j = 0; j != local_execution.nodes_size(); ++j) {
        SERVING_ENFORCE(remote_execution.nodes(j) == local_execution.nodes(j),
                        serving::errors::LOGIC_ERROR,
                        "node name mismatch: {}: {}, local: {}: {}",
                        remote_party_id, remote_execution.nodes(j),
                        local_party_id_, local_execution.nodes(j));
      }

      if (remote_execution.config().dispatch_type() ==
          DispatchType::DP_SPECIFIED) {
        if (remote_execution.config().specific_flag()) {
          SERVING_ENFORCE(specific_map_[i].empty(),
                          serving::errors::LOGIC_ERROR,
                          "{} execution specific to multiple parties", i);
          specific_map_[i] = remote_party_id;
        }
      }
    }
  }

  for (auto& [id, party_id] : specific_map_) {
    SERVING_ENFORCE(!party_id.empty(), serving::errors::LOGIC_ERROR,
                    "{} execution specific to no party", id);
  }
}
void ModelInfoProcessor::CheckNodeViewList(
    const ::google::protobuf::RepeatedPtrField<::secretflow::serving::NodeView>&
        remote_node_views,
    const std::string& remote_party_id) {
  SERVING_ENFORCE_EQ(
      local_node_views_.size(), static_cast<size_t>(remote_node_views.size()),
      "node views size is not equal, {} : {}, {} : {}", local_party_id_,
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
                       remote_node_view.name(), local_party_id_,
                       local_node_view.op(), remote_party_id,
                       remote_node_view.op());
    SERVING_ENFORCE_EQ(
        local_node_view.op_version(), remote_node_view.op_version(),
        "node view {} op version is not equal, {} : {}, {} : {}",
        remote_node_view.name(), local_party_id_, local_node_view.op_version(),
        remote_party_id, remote_node_view.op_version());
    auto remote_node_parents = std::set<std::string>(
        remote_node_view.parents().begin(), remote_node_view.parents().end());
    SERVING_ENFORCE(
        local_node_parents == remote_node_parents, serving::errors::LOGIC_ERROR,
        "node view {} op parents is not equal, {} : {}, {} : {}",
        remote_node_view.name(), local_party_id_,
        fmt::join(local_node_parents.begin(), local_node_parents.end(), ","),
        remote_party_id,
        fmt::join(remote_node_parents.begin(), remote_node_parents.end(), ","));
  }
}

}  // namespace secretflow::serving
