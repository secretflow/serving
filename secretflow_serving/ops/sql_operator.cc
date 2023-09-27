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

#include "secretflow_serving/ops/sql_operator.h"

#include <set>

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

SqlOperator::SqlOperator(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  tbl_name_ = GetNodeAttr<std::string>(opts_.node->node_def(), "tbl_name");
  input_feature_names_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "input_feature_names");
  input_feature_types_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "input_feature_types");
  output_feature_names_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "output_feature_names");
  output_feature_types_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "output_feature_types");
  sql_ = GetNodeAttr<std::string>(opts_.node->node_def(), "sql");
  if (sql_.empty() || std::all_of(sql_.begin(), sql_.end(), ::isspace)) {
    is_compute_run_ = false;
    SPDLOG_INFO("the input sql is empty, skip the comput process");
  } else {
    is_compute_run_ = true;
  }
  BuildInputSchema();
  BuildOutputSchema();
}

void SqlOperator::Compute(ComputeContext* ctx) {
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
  secretflow::serving::ops::sql::SqlProcessor sql_processor;
  std::unordered_map<std::string, std::shared_ptr<arrow::Array>> data_map =
      sql_processor.GetSqlResult(sql_, tbl_name_, input_feature_names_,
                                 input_feature_types_, *input_table);
  for (int i = 0; i < input_table->num_columns(); i++) {
    data_map[input_table->column_name(i)] = input_table->column(i);
  }
  std::vector<std::shared_ptr<arrow::Array>> data_result;
  for (auto field_name : output_schema_->field_names()) {
    data_result.push_back(data_map[field_name]);
  }
  ctx->output =
      MakeRecordBatch(output_schema_, input_table->num_rows(), data_result);
}

void SqlOperator::BuildInputSchema() {
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

void SqlOperator::BuildOutputSchema() {
  SERVING_ENFORCE(
      output_feature_types_.size() == output_feature_names_.size(),
      errors::ErrorCode::INVALID_ARGUMENT,
      "the name size and type size of output feature are unmatched");
  // build output schema
  std::vector<std::shared_ptr<arrow::Field>> f_list;
  for (size_t i = 0; i < output_feature_types_.size(); i++) {
    std::string feature_type = output_feature_types_[i];
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

REGISTER_OP_KERNEL(SQL_OPERATOR, SqlOperator)
REGISTER_OP(SQL_OPERATOR, "0.1.0", "test")
    .StringAttr("tbl_name", "", false, false)
    .StringAttr("input_feature_names", "", true, false)
    .StringAttr("input_feature_types", "", true, false)
    .StringAttr("output_feature_names", "", true, false)
    .StringAttr("output_feature_types", "", true, false)
    .StringAttr("sql", "", false, false)
    .Input("input_table", "")
    .Output("output_table", "");
}  // namespace secretflow::serving::op