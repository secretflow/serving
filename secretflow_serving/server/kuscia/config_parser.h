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

#include <optional>

#include "secretflow_serving/config/cluster_config.pb.h"
#include "secretflow_serving/config/feature_config.pb.h"
#include "secretflow_serving/config/model_config.pb.h"
#include "secretflow_serving/config/server_config.pb.h"

namespace secretflow::serving::kuscia {

class KusciaConfigParser {
 public:
  explicit KusciaConfigParser(const std::string& config_file);
  ~KusciaConfigParser() = default;

  std::string service_id() const { return service_id_; }

  ClusterConfig cluster_config() const { return cluster_config_; }

  ModelConfig model_config() const { return model_config_; }

  ServerConfig server_config() const { return server_config_; }

  std::optional<FeatureSourceConfig> feature_config() {
    return feature_config_;
  }

 private:
  std::string service_id_;

  ClusterConfig cluster_config_;
  ServerConfig server_config_;
  ModelConfig model_config_;

  std::optional<FeatureSourceConfig> feature_config_;
};

}  // namespace secretflow::serving::kuscia
