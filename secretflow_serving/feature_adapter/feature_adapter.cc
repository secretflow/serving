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

#include "secretflow_serving/feature_adapter/feature_adapter.h"

#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::feature {

FeatureAdapter::FeatureAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : spec_(spec),
      service_id_(service_id),
      party_id_(party_id),
      feature_schema_(feature_schema) {}

void FeatureAdapter::FetchFeature(const Request& request, Response* response) {
  OnFetchFeature(request, response);

  CheckFeatureValid(request, response->features);
}

void FeatureAdapter::CheckFeatureValid(
    const Request& request,
    const std::shared_ptr<arrow::RecordBatch>& features) {
  const auto& schema = features->schema();
  if (feature_schema_->num_fields() > 0) {
    SERVING_ENFORCE(schema->Equals(*feature_schema_),
                    errors::ErrorCode::NOT_FOUND,
                    "result schema does not match the request expect.");
  }
  SERVING_ENFORCE(
      request.fs_param->query_datas().size() == features->num_rows(),
      errors::ErrorCode::LOGIC_ERROR,
      "query row_num {} should be equal to fetched row_num {}",
      request.fs_param->query_datas().size(), features->num_rows());
}

}  // namespace secretflow::serving::feature
