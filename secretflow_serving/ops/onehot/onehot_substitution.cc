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

#include "secretflow_serving/ops/onehot/onehot_substitution.h"

#include <fmt/format.h>
#include <json2pb/json_to_pb.h>

#include <iomanip>
#include <iostream>

#include "absl/strings/escaping.h"
#include "butil/file_util.h"
#include "google/protobuf/message.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/protos/onehot_substitution_spec.pb.h"

namespace {

const std::string kPositive = "1";
const std::string kNegtive = "0";

}  // namespace

namespace secretflow::serving::ops::onehot {

OneHotSubstitution::OneHotSubstitution(const std::string &json_content) {
  secretflow::serving::OneHotSubstitutionSpec onehot_sub_spec;
  secretflow::serving::JsonToPb(json_content, &onehot_sub_spec);
  LoadOnehotMapFromPb(onehot_sub_spec);
}

void OneHotSubstitution::LoadOnehotMapFromPb(
    const secretflow::serving::OneHotSubstitutionSpec &onehot_sub_spec) {
  std::unordered_map<std::string, VariableOneHotMeta> *var_onehot_map_temp =
      new std::unordered_map<std::string, VariableOneHotMeta>();
  ;
  for (const auto &feature_mapping : onehot_sub_spec.feature_mappings()) {
    VariableOneHotMeta var_onehot_meta;
    var_onehot_meta.raw_name = feature_mapping.raw_name();
    for (int i = 0; i < feature_mapping.categories_size(); i++) {
      const auto &category = feature_mapping.categories(i);
      var_onehot_meta.categories.emplace_back(
          Category{category.mapping_name(), category.value()});
    }
    var_onehot_map_temp->insert(
        std::make_pair(feature_mapping.raw_name(), std::move(var_onehot_meta)));
  }
  var_onehot_map_.reset(var_onehot_map_temp);
}

std::vector<OneHotSubstitution::Category> OneHotSubstitution::Encode(
    const std::string &variable_name, const std::string &val) const {
  auto iter = var_onehot_map_->find(variable_name);
  if (iter == var_onehot_map_->end()) {
    Category category{variable_name, val};
    return std::vector<Category>(1, category);
  }

  const auto &onehot_meta = iter->second;
  std::vector<Category> categories;
  categories.resize(onehot_meta.categories.size());
  const std::vector<std::string> encoding_result = iter->second.Encode(val);
  size_t index = 0;
  std::transform(
      onehot_meta.categories.begin(), onehot_meta.categories.end(),
      categories.begin(), [&](const auto &category) {
        Category cate = {category.mapping_name, encoding_result[index]};
        index++;
        return cate;
      });
  return categories;
}

std::shared_ptr<arrow::RecordBatch> OneHotSubstitution::ApplyTo(
    const std::shared_ptr<arrow::RecordBatch> &batch,
    const std::shared_ptr<arrow::Schema> &output_schema) {
  std::unordered_map<std::string, std::shared_ptr<arrow::Array>> result_map;
  for (int i = 0; i < batch->num_columns(); ++i) {
    std::string key = batch->column_name(i);
    auto col = batch->column(i);
    auto iter = var_onehot_map_->find(key);
    if (iter == var_onehot_map_->end()) {
      // key does not exist
      result_map.insert(std::make_pair(std::move(key), std::move(col)));
    } else {
      // key exists
      std::unordered_map<std::string, std::shared_ptr<arrow::DoubleBuilder>>
          builder_map;
      const auto &onehot_meta = iter->second;
      for (auto category : onehot_meta.categories) {
        std::shared_ptr<arrow::DoubleBuilder> builder =
            std::make_shared<arrow::DoubleBuilder>();
        builder_map.insert(std::make_pair(category.mapping_name, builder));
      }
      for (int64_t j = 0; j < col->length(); ++j) {
        std::shared_ptr<arrow::Scalar> raw_scalar;
        SERVING_GET_ARROW_RESULT(col->GetScalar(j), raw_scalar);
        std::string value;
        if (col->type_id() == arrow::Type::type::STRING) {
          value =
              std::static_pointer_cast<arrow::StringScalar>(raw_scalar)->view();
        } else if (col->type_id() == arrow::Type::type::DOUBLE) {
          std::ostringstream double_to_string;
          double_to_string << std::setprecision(20)
                           << std::static_pointer_cast<arrow::DoubleScalar>(
                                  raw_scalar)
                                  ->value;
          value = double_to_string.str();
        } else {
          SERVING_THROW(
              secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT,
              "unknow feature type: {}", col->type_id());
        }
        auto categories = Encode(key, value);
        std::for_each(categories.begin(), categories.end(),
                      [&](auto &category) {
                        SERVING_CHECK_ARROW_STATUS(
                            builder_map[category.mapping_name]->Append(
                                std::stod(category.value)));
                      });
      }
      for (auto &kv : builder_map) {
        std::shared_ptr<arrow::Array> array;
        SERVING_CHECK_ARROW_STATUS(builder_map[kv.first]->Finish(&array));
        result_map.insert(
            std::make_pair(std::move(kv.first), std::move(array)));
      }
    }
  }
  std::vector<std::shared_ptr<arrow::Array>> result_list;
  for (auto field_name : output_schema->field_names()) {
    result_list.push_back(result_map[field_name]);
  }
  return MakeRecordBatch(output_schema, batch->num_rows(), result_list);
}

std::vector<std::string> OneHotSubstitution::VariableOneHotMeta::Encode(
    const std::string &raw_value) const {
  absl::string_view value = absl::StripAsciiWhitespace(raw_value);
  std::vector<std::string> encoding_result(categories.size(), kNegtive);
  const auto iter = std::find_if(
      categories.begin(), categories.end(),
      [&](const auto &category) { return category.value == value; });
  if (iter != categories.end()) {
    encoding_result[iter - categories.begin()] = kPositive;
  }
  return encoding_result;
}

}  // namespace secretflow::serving::ops::onehot