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

#include "secretflow_serving/server/prediction_core.h"

#include "spdlog/spdlog.h"

namespace secretflow::serving {

PredictionCore::PredictionCore(Options opts) : opts_(std::move(opts)) {
  SERVING_ENFORCE(!opts_.service_id.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(!opts_.party_id.empty(), errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(!opts_.cluster_ids.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(opts_.predictor, errors::ErrorCode::INVALID_ARGUMENT);
}

void PredictionCore::Predict(const apis::PredictRequest* request,
                             apis::PredictResponse* response) noexcept {
  try {
    SERVING_ENFORCE(request->service_spec().id() == opts_.service_id,
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "invalid service sepc id: {}",
                    request->service_spec().id());
    response->mutable_service_spec()->CopyFrom(request->service_spec());
    auto status = response->mutable_status();

    CheckArgument(request);
    opts_.predictor->Predict(request, response);
    status->set_code(errors::ErrorCode::OK);
  } catch (const Exception& e) {
    SPDLOG_ERROR("predict failed, code:{}, msg:{}, stack:{}", e.code(),
                 e.what(), e.stack_trace());
    response->mutable_status()->set_code(e.code());
    response->mutable_status()->set_msg(e.what());
  } catch (const std::exception& e) {
    SPDLOG_ERROR("predict failed, msg:{}", e.what());
    response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
    response->mutable_status()->set_msg(e.what());
  }
}

void PredictionCore::CheckArgument(const apis::PredictRequest* request) {
  SERVING_ENFORCE(request->service_spec().id() == opts_.service_id,
                  errors::ErrorCode::INVALID_ARGUMENT, "invalid service id: {}",
                  request->service_spec().id());

  std::vector<std::string> missing_params_party;
  for (const auto& party_id : opts_.cluster_ids) {
    auto it = request->fs_params().find(party_id);
    if (it == request->fs_params().end()) {
      if (party_id == opts_.party_id &&
          !request->predefined_features().empty()) {
        // check whether set predefined features
        continue;
      }
      missing_params_party.emplace_back(party_id);
    }
  }
  if (!missing_params_party.empty()) {
    SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                  "{} missing feature params",
                  fmt::join(missing_params_party.begin(),
                            missing_params_party.end(), ","));
  }
}

}  // namespace secretflow::serving
