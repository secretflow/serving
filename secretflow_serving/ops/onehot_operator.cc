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

#include "secretflow_serving/ops/onehot_operator.h"

#include <iomanip>
#include <iostream>
#include <set>

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

OnehotOperator::OnehotOperator(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  rule_json_ = GetNodeAttr<std::string>(opts_.node->node_def(), "rule_json");
  input_feature_names_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "input_feature_names");
  input_feature_types_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "input_feature_types");
  output_feature_names_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "output_feature_names");
  output_feature_types_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "output_feature_types");
  if (rule_json_.empty() ||
      std::all_of(rule_json_.begin(), rule_json_.end(), ::isspace)) {
    is_compute_run_ = false;
    SPDLOG_INFO("the input onehot rule_json is empty, skip the comput process");
  } else {
    onehot_substitution_.reset(
        new secretflow::serving::ops::onehot::OneHotSubstitution(
            std::move(rule_json_)));
    is_compute_run_ = true;
  }
  BuildInputSchema();
  BuildOutputSchema();
}

void OnehotOperator::Compute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs->size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs->front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  auto input_table = ctx->inputs->front()[0];
  SERVING_ENFORCE(input_table->schema()->Equals(input_schema_list_.front()),
                  errors::ErrorCode::LOGIC_ERROR);
  if (!is_compute_run_) {
    ctx->output = input_table;
    return;
  }
  ctx->output = onehot_substitution_->ApplyTo(input_table, output_schema_);
}

void OnehotOperator::BuildInputSchema() {
  SERVING_ENFORCE(input_feature_types_.size() == input_feature_names_.size(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "the name size and type size of input feature are unmatched");
  // build input schema
  std::vector<std::shared_ptr<arrow::Field>> f_list;
  for (size_t i = 0; i < input_feature_types_.size(); i++) {
    std::string feature_type = input_feature_types_[i];
    if (feature_type == "string") {
      f_list.emplace_back(arrow::field(input_feature_names_[i], arrow::utf8()));
    } else if (feature_type == "double") {
      f_list.emplace_back(
          arrow::field(input_feature_names_[i], arrow::float64()));
    } else {
      SERVING_THROW(secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT,
                    "unknow feature type: {}", feature_type);
    }
  }
  input_schema_list_.emplace_back(arrow::schema(std::move(f_list)));
}

void OnehotOperator::BuildOutputSchema() {
  SERVING_ENFORCE(
      output_feature_types_.size() == output_feature_names_.size(),
      errors::ErrorCode::INVALID_ARGUMENT,
      "the name size and type size of output feature are unmatched");
  // build output schema
  std::vector<std::shared_ptr<arrow::Field>> f_list;
  for (size_t i = 0; i < output_feature_types_.size(); i++) {
    std::string feature_type = output_feature_types_[i];
    output_feature_name_type_map_[output_feature_names_[i]] = feature_type;
    if (feature_type == "string") {
      f_list.emplace_back(
          arrow::field(output_feature_names_[i], arrow::utf8()));
    } else if (feature_type == "double") {
      f_list.emplace_back(
          arrow::field(output_feature_names_[i], arrow::float64()));
    } else {
      SERVING_THROW(secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT,
                    "unknow feature type: {}", feature_type);
    }
  }
  output_schema_ = arrow::schema(std::move(f_list));
}

REGISTER_OP_KERNEL(ONEHOT_OPERATOR, OnehotOperator)
REGISTER_OP(ONEHOT_OPERATOR, "0.1.0", "one hot encode")
    .StringAttr("rule_json", "", false, false)
    .StringAttr("input_feature_names", "", true, false)
    .StringAttr("input_feature_types", "", true, false)
    .StringAttr("output_feature_names", "", true, false)
    .StringAttr("output_feature_types", "", true, false)
    .Input("input_table", "")
    .Output("output_table", "");
}  // namespace secretflow::serving::op
