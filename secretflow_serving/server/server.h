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

#include <memory>
#include <optional>
#include <string>

#include "brpc/server.h"

#include "secretflow_serving/server/health.h"
#include "secretflow_serving/server/model_service_impl.h"
#include "secretflow_serving/server/prediction_core.h"

#include "secretflow_serving/config/cluster_config.pb.h"
#include "secretflow_serving/config/feature_config.pb.h"
#include "secretflow_serving/config/model_config.pb.h"
#include "secretflow_serving/config/server_config.pb.h"

namespace secretflow::serving {

class Server {
 public:
  struct Options {
    std::string service_id;

    ServerConfig server_config;
    ClusterConfig cluster_config;
    ModelConfig model_config;

    std::optional<FeatureSourceConfig> feature_source_config;
  };

 public:
  explicit Server(Options opts);
  ~Server();

  void Start(std::shared_ptr<std::map<
                 std::string, std::unique_ptr<::google::protobuf::RpcChannel>>>
                 channels,
             google::protobuf::Service* additional_service = nullptr);

  // This will block the current thread until termination is successful.
  void WaitForEnd();

  std::shared_ptr<PredictionCore> GetPredictionCore() {
    return prediction_core_;
  }

 private:
  const Options opts_;

  brpc::Server service_server_;
  brpc::Server communication_server_;
  brpc::Server metrics_server_;

  std::unique_ptr<health::ServingHealthReporter> hr_;

  std::unique_ptr<ModelServiceImpl> model_service_;

  std::shared_ptr<PredictionCore> prediction_core_;
};

}  // namespace secretflow::serving
