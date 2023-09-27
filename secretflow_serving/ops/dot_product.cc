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

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

namespace {

// 将arrow::RecordBatch数据导入到Eigen::Matrix中
Double::Matrix TableToMatrix(const std::shared_ptr<arrow::RecordBatch>& table) {
  int rows = table->num_rows();
  int cols = table->num_columns();

  Double::Matrix matrix;
  matrix.resize(rows, cols);
  Eigen::Map<Double::Matrix> map(matrix.data(), rows, cols);

  // 遍历table的每一列，将数据映射到Eigen::Matrix中
  for (int i = 0; i < cols; ++i) {
    auto col = table->column(i);
    // index 0 is validity bitmap, real data start with 1
    auto data = col->data()->GetMutableValues<double>(1);
    SERVING_ENFORCE(data, errors::ErrorCode::LOGIC_ERROR,
                    "found unsupport field type");
    Eigen::Map<Eigen::VectorXd> vec(data, rows);
    map.col(i) = vec;
  }

  return matrix;
}

}  // namespace

DotProduct::DotProduct(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node->node_def(), "output_col_name");
  // optional attr
  GetNodeAttr(opts_.node->node_def(), "intercept", &intercept_);

  // feature
  std::set<std::string> f_name_set;
  feature_name_list_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node->node_def(), "feature_names");
  for (auto& feature_name : feature_name_list_) {
    SERVING_ENFORCE(f_name_set.emplace(feature_name).second,
                    errors::ErrorCode::LOGIC_ERROR);
  }
  auto feature_weight_list = GetNodeAttr<std::vector<double>>(
      opts_.node->node_def(), "feature_weights");

  SERVING_ENFORCE(feature_name_list_.size() == feature_weight_list.size(),
                  errors::ErrorCode::UNEXPECTED_ERROR,
                  "attr:feature_names size={} does not match "
                  "attr:feature_weights size={}, node:{}, op:{}",
                  feature_name_list_.size(), feature_weight_list.size(),
                  opts_.node->node_def().name(), opts_.node->node_def().op());

  weights_ = Double::ColVec::Zero(feature_weight_list.size());
  for (size_t i = 0; i < feature_weight_list.size(); i++) {
    weights_[i] = feature_weight_list[i];
  }

  BuildInputSchema();
  BuildOutputSchema();
}

void DotProduct::Compute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs->size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs->front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  auto input_table = ctx->inputs->front()[0];
  SERVING_ENFORCE(input_table->schema()->Equals(input_schema_list_.front()),
                  errors::ErrorCode::LOGIC_ERROR);

  auto features = TableToMatrix(input_table);

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
  int inputs_size = opts_.node->GetOpDef()->inputs_size();
  for (int i = 0; i < inputs_size; ++i) {
    std::vector<std::shared_ptr<arrow::Field>> f_list;
    for (const auto& f : feature_name_list_) {
      f_list.emplace_back(arrow::field(f, arrow::float64()));
    }
    input_schema_list_.emplace_back(arrow::schema(std::move(f_list)));
    // should only have 1 input
    break;
  }
}

void DotProduct::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(DOT_PRODUCT, DotProduct)
REGISTER_OP(DOT_PRODUCT, "0.0.1",
            "Calculate the dot product of feature weights and values")
    .StringAttr("feature_names", "", true, false)
    .DoubleAttr("feature_weights", "", true, false)
    .StringAttr("output_col_name", "", false, false)
    .DoubleAttr("intercept", "", false, true, 0.0d)
    .Input("features", "")
    .Output("ys", "");

}  // namespace secretflow::serving::op
