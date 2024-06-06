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

#pragma once

#include <unordered_map>

#include "secretflow_serving/protos/bundle.pb.h"

namespace secretflow::serving {

class ModelInfoProcessor {
 public:
  ModelInfoProcessor(
      const std::string local_party_id, const ModelInfo& local_model_info,
      const std::unordered_map<std::string, ModelInfo>& remote_model_info);

  std::unordered_map<size_t, std::string> GetSpecificMap();

 private:
  void CheckAndSetSpecificMap();

  void CheckNodeViewList(
      const ::google::protobuf::RepeatedPtrField<NodeView>& remote_node_views,
      const std::string& remote_party_id);

 private:
  std::string local_party_id_;

  // key: execution_id(index), value: party_id
  std::unordered_map<size_t, std::string> specific_map_;

  // key: node name, value: <node view, parents>
  std::unordered_map<std::string, std::pair<NodeView, std::set<std::string>>>
      local_node_views_;

  const ModelInfo* local_model_info_ = nullptr;
  const std::unordered_map<std::string, ModelInfo>* remote_model_info_ =
      nullptr;
};

}  // namespace secretflow::serving
