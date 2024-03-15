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

#include "secretflow_serving/feature_adapter/file_adapter.h"

#include "arrow/compute/api.h"
#include "arrow/csv/api.h"
#include "arrow/io/api.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::feature {

FileAdapter::FileAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : FeatureAdapter(spec, service_id, party_id, feature_schema),
      extractor_(feature_schema, spec_.csv_opts().file_path(),
                 spec_.csv_opts().id_name()) {
  SERVING_ENFORCE(spec_.has_csv_opts(), errors::ErrorCode::INVALID_ARGUMENT,
                  "invalid mock options");
}

void FileAdapter::OnFetchFeature(const Request& request, Response* response) {
  response->features =
      extractor_.ExtractRows(feature_schema_, request.fs_param->query_datas());
}

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kCsvOpts, FileAdapter);

}  // namespace secretflow::serving::feature
