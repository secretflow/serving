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

#include "secretflow_serving/ops/dot_product.h"

#include <set>

#include "arrow/compute/api.h"

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/data_type.pb.h"

namespace secretflow::serving::op {

namespace {

// 将arrow::RecordBatch数据导入到Eigen::Matrix中
Double::Matrix TableToMatrix(const std::shared_ptr<arrow::RecordBatch>& table) {
  int rows = table->num_rows();
  int cols = table->num_columns();

  Double::Matrix matrix;
  matrix.resize(rows, cols);

  // 遍历table的每一列，将数据映射到Eigen::Matrix中
  for (int i = 0; i < cols; ++i) {
    const auto& col = table->column(i);
    std::shared_ptr<arrow::Array> double_array = col;
    if (col->type_id() != arrow::Type::DOUBLE) {
      arrow::Datum double_array_datum;
      SERVING_GET_ARROW_RESULT(
          arrow::compute::Cast(
              col, arrow::compute::CastOptions::Safe(arrow::float64())),
          double_array_datum);
      double_array = std::move(double_array_datum).make_array();
    }

    // index 0 is validity bitmap, real data start with 1
    auto data = double_array->data()->GetMutableValues<double>(1);
    SERVING_ENFORCE(data, errors::ErrorCode::LOGIC_ERROR,
                    "found unsupported field type");
    Eigen::Map<Eigen::VectorXd> vec(data, rows);
    matrix.col(i) = vec;
  }

  return matrix;
}

}  // namespace

DotProduct::DotProduct(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  SERVING_ENFORCE_EQ(opts_.node_def.op_version(), "0.0.2");

  // feature name
  feature_name_list_ =
      GetNodeAttr<std::vector<std::string>>(opts_.node_def, "feature_names");
  std::set<std::string> f_name_set;
  for (auto& feature_name : feature_name_list_) {
    SERVING_ENFORCE(f_name_set.emplace(feature_name).second,
                    errors::ErrorCode::LOGIC_ERROR,
                    "found duplicate feature name:{}", feature_name);
  }
  // feature types
  feature_type_list_ =
      GetNodeAttr<std::vector<std::string>>(opts_.node_def, "input_types");
  SERVING_ENFORCE_EQ(feature_name_list_.size(), feature_type_list_.size(),
                     "attr:feature_names size={} does not match "
                     "attr:input_types size={}, node:{}, op:{}",
                     feature_name_list_.size(), feature_type_list_.size(),
                     opts_.node_def.name(), opts_.node_def.op());

  auto feature_weight_list =
      GetNodeAttr<std::vector<double>>(opts_.node_def, "feature_weights");
  SERVING_ENFORCE_EQ(feature_name_list_.size(), feature_weight_list.size(),
                     "attr:feature_names size={} does not match "
                     "attr:feature_weights size={}, node:{}, op:{}",
                     feature_name_list_.size(), feature_weight_list.size(),
                     opts_.node_def.name(), opts_.node_def.op());

  weights_ = Double::ColVec::Zero(feature_weight_list.size());
  for (size_t i = 0; i < feature_weight_list.size(); i++) {
    weights_[i] = feature_weight_list[i];
  }

  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");

  // optional attr
  GetNodeAttr(opts_.node_def, "intercept", &intercept_);

  BuildInputSchema();
  BuildOutputSchema();
}

void DotProduct::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  auto features = TableToMatrix(ctx->inputs.front().front());

  Double::ColVec score_vec = features * weights_;
  score_vec.array() += intercept_;

  std::shared_ptr<arrow::Array> array;
  arrow::DoubleBuilder builder;
  for (int i = 0; i < score_vec.rows(); ++i) {
    auto row = score_vec.row(i);
    SERVING_CHECK_ARROW_STATUS(builder.AppendValues(row.data(), 1));
  }
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
  ctx->output = MakeRecordBatch(output_schema_, score_vec.rows(), {array});
}

void DotProduct::BuildInputSchema() {
  // build input schema
  std::vector<std::shared_ptr<arrow::Field>> fields;
  for (size_t i = 0; i < feature_name_list_.size(); ++i) {
    auto data_type = DataTypeToArrowDataType(feature_type_list_[i]);
    SERVING_ENFORCE(
        arrow::is_numeric(data_type->id()), errors::INVALID_ARGUMENT,
        "feature type must be numeric, get:{}", feature_type_list_[i]);
    fields.emplace_back(arrow::field(feature_name_list_[i], data_type));
  }
  input_schema_list_.emplace_back(arrow::schema(std::move(fields)));
}

void DotProduct::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(DOT_PRODUCT, DotProduct)
REGISTER_OP(DOT_PRODUCT, "0.0.2",
            "Calculate the dot product of feature weights and values")
    .StringAttr("feature_names", "List of feature names", true, false)
    .DoubleAttr("feature_weights", "List of feature weights", true, false)
    .StringAttr("input_types",
                "List of input feature data types, Note that there is a loss "
                "of precision when using `DT_FLOAT` type. Optional "
                "value: DT_UINT8, "
                "DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, "
                "DT_INT64, DT_FLOAT, DT_DOUBLE",
                true, false)
    .StringAttr("output_col_name", "Column name of partial y", false, false)
    .DoubleAttr("intercept", "Value of model intercept", false, true, 0.0d)
    .Input("features", "Input feature table")
    .Output("partial_ys",
            "The calculation results, they have a data type of `double`.");

}  // namespace secretflow::serving::op
