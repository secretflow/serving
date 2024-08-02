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

#include "brpc/controller.h"

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/protos/bundle.pb.h"

namespace secretflow::serving {

class ModelInfoCollector {
 public:
  struct Options {
    std::string self_party_id;
    std::string service_id;

    std::shared_ptr<ModelBundle> model_bundle;

    std::shared_ptr<
        std::map<std::string, std::shared_ptr<::google::protobuf::RpcChannel>>>
        remote_channel_map;
  };

 public:
  explicit ModelInfoCollector(Options opts);

  void DoCollect();

  const ModelInfo& GetSelfModelInfo() { return model_info_; }

  void SetRetryCounts(uint32_t max_retry_cnt) {
    max_retry_cnt_ = max_retry_cnt;
  }
  void SetRetryIntervalMs(uint32_t retry_interval_ms) {
    retry_interval_ms_ = retry_interval_ms;
  }

  std::unordered_map<size_t, std::string> GetSpecificMap() const {
    return specific_party_map_;
  }

 private:
  bool TryCollect(
      const std::string& remote_party_id,
      const std::shared_ptr<::google::protobuf::RpcChannel>& channel);

  void CheckAndSetSpecificMap();

  void CheckNodeViewList(
      const ::google::protobuf::RepeatedPtrField<
          ::secretflow::serving::NodeView>& remote_node_views,
      const std::string& remote_party_id);

 private:
  Options opts_;

  ModelInfo model_info_;

  // key: execution_id(index), value: party_id
  std::unordered_map<size_t, std::string> specific_party_map_;

  // key: node name, value: <node view, parents>
  std::unordered_map<std::string, std::pair<NodeView, std::set<std::string>>>
      local_node_views_;

  std::unordered_map<std::string, ModelInfo> model_info_map_;

  uint32_t max_retry_cnt_{60};
  uint32_t retry_interval_ms_{5000};
};

}  // namespace secretflow::serving
