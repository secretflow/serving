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

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/framework/predictor.h"

#include "secretflow_serving/apis/prediction_service.pb.h"

namespace secretflow::serving {

class PredictionCore {
 public:
  struct Options {
    std::string service_id;
    std::string party_id;
    std::vector<std::string> cluster_ids;
    std::shared_ptr<Predictor> predictor;
  };

 public:
  explicit PredictionCore(Options opts);
  ~PredictionCore() = default;

  void Predict(const apis::PredictRequest* request,
               apis::PredictResponse* response) noexcept;

  const std::string& GetServiceID() const { return opts_.service_id; }
  const std::string& GetPartyID() const { return opts_.party_id; }

 protected:
  void CheckArgument(const apis::PredictRequest* request);

 private:
  const Options opts_;
};

}  // namespace secretflow::serving
