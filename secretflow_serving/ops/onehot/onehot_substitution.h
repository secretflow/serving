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

#include <arrow/api.h>
#include <arrow/dataset/api.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "secretflow_serving/protos/onehot_substitution_spec.pb.h"

namespace secretflow::serving::ops::onehot {

using RawUserFeatureMap = std::unordered_map<std::string, std::string>;

class OneHotSubstitution {
 public:
  struct Category {
    std::string mapping_name;
    std::string value;
  };

 public:
  OneHotSubstitution(const std::string& json_content);
  std::shared_ptr<arrow::RecordBatch> ApplyTo(
      const std::shared_ptr<arrow::RecordBatch>& batch,
      const std::shared_ptr<arrow::Schema>& output_schema);

 private:
  struct VariableOneHotMeta {
    std::string raw_name;
    std::vector<Category> categories;
    std::vector<std::string> Encode(const std::string& raw_value) const;
  };

  void LoadOnehotMapFromPb(
      const secretflow::serving::OneHotSubstitutionSpec& onehot_sub_spec);
  std::vector<Category> Encode(const std::string& variable_name,
                               const std::string& raw_val) const;

 private:
  std::unique_ptr<std::unordered_map<std::string, VariableOneHotMeta>>
      var_onehot_map_;
};

}  // namespace secretflow::serving::ops::onehot
