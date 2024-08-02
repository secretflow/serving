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

#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace {

struct FeatureLengthVisitor {
  template <typename Vec>
  void operator()(const FeatureField& field, const Vec& values) {
    len = values.size();
    field_name = field.name();
  }
  int len = 0;
  std::string field_name;
};

}  // namespace

PredictionCore::PredictionCore(Options opts) : opts_(std::move(opts)) {
  SERVING_ENFORCE(!opts_.service_id.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(!opts_.party_id.empty(), errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(!opts_.cluster_ids.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT);
}

void PredictionCore::Predict(const apis::PredictRequest* request,
                             apis::PredictResponse* response) noexcept {
  try {
    response->mutable_service_spec()->CopyFrom(request->service_spec());
    auto* status = response->mutable_status();

    CheckArgument(request);
    opts_.predictor->Predict(request, response);
    status->set_code(errors::ErrorCode::OK);
  } catch (const Exception& e) {
    SPDLOG_ERROR("Predict failed, code:{}, msg:{}, stack:{}", e.code(),
                 e.what(), e.stack_trace());
    response->mutable_status()->set_code(e.code());
    response->mutable_status()->set_msg(e.what());
  } catch (const std::exception& e) {
    SPDLOG_ERROR("Predict failed, msg:{}", e.what());
    response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
    response->mutable_status()->set_msg(e.what());
  }
}

void PredictionCore::CheckArgument(const apis::PredictRequest* request) {
  SERVING_ENFORCE_EQ(request->service_spec().id(), opts_.service_id,
                     "invalid service spec id: {}",
                     request->service_spec().id());
  std::vector<std::string> missing_params_party;
  std::unordered_map<std::string, size_t> party_row_num;
  for (const auto& party_id : opts_.cluster_ids) {
    if (party_id == opts_.party_id && !request->predefined_features().empty()) {
      int predefined_row_num = -1;
      for (const auto& feature : request->predefined_features()) {
        FeatureLengthVisitor len_visitor;
        FeatureVisit(len_visitor, feature);

        if (predefined_row_num == -1) {
          predefined_row_num = len_visitor.len;
        } else {
          SERVING_ENFORCE(predefined_row_num == len_visitor.len,
                          errors::ErrorCode::INVALID_ARGUMENT,
                          "predifined_features should have same length, {} : "
                          "{}, previous is {}",
                          len_visitor.field_name, len_visitor.len,
                          predefined_row_num);
        }
      }
      party_row_num[party_id] = predefined_row_num;
      continue;
    }

    auto it = request->fs_params().find(party_id);

    if (it == request->fs_params().end()) {
      missing_params_party.emplace_back(party_id);
    } else {
      if (it->second.query_datas().empty()) {
        missing_params_party.emplace_back(party_id);
      } else {
        party_row_num[party_id] = it->second.query_datas().size();
      }
    }
  }
  if (!missing_params_party.empty()) {
    SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                  "{} missing feature params or got empty query datas",
                  fmt::join(missing_params_party.begin(),
                            missing_params_party.end(), ","));
  }
  auto row_num_iter = party_row_num.begin();
  auto row_num = row_num_iter->second;
  for (++row_num_iter; row_num_iter != party_row_num.end(); ++row_num_iter) {
    SERVING_ENFORCE(row_num == row_num_iter->second,
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "predict row nums should be same, expect:{}, "
                    "party({}) : {}",
                    row_num, row_num_iter->first, row_num_iter->second);
  }
}

}  // namespace secretflow::serving
