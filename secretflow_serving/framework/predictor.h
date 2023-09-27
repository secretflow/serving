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

#include <map>
#include <memory>

#include "google/protobuf/service.h"

#include "secretflow_serving/server/execution_core.h"

#include "secretflow_serving/apis/prediction_service.pb.h"

namespace secretflow::serving {

// key: node_id
// value: channel to the executor
using PartyChannelMap =
    std::map<std::string, std::shared_ptr<::google::protobuf::RpcChannel>>;

class Predictor {
 public:
  struct Options {
    std::string party_id;

    std::shared_ptr<PartyChannelMap> channels;

    std::vector<std::shared_ptr<Execution>> executions;
  };

 public:
  explicit Predictor(Options opts) : opts_(std::move(opts)) {}
  virtual ~Predictor() = default;

  virtual void Predict(const apis::PredictRequest* request,
                       apis::PredictResponse* response) = 0;

  void SetExecutionCore(std::shared_ptr<ExecutionCore>& execution_core) {
    execution_core_ = execution_core;
  }

 protected:
  Options opts_;

  std::shared_ptr<ExecutionCore> execution_core_;
};

}  // namespace secretflow::serving
