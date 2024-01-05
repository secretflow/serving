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

#include "secretflow_serving/ops/merge_y.h"

#include <set>

#include "arrow/compute/api.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

MergeY::MergeY(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  auto link_function_name =
      GetNodeAttr<std::string>(opts_.node_def, "link_function");
  link_function_ = ParseLinkFuncType(link_function_name);

  // optional attr
  GetNodeAttr(opts_.node_def, "yhat_scale", &yhat_scale_);

  input_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "input_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");

  BuildInputSchema();
  BuildOutputSchema();
}

void MergeY::DoCompute(ComputeContext* ctx) {
  // santiy check
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() >= 1,
                  errors::ErrorCode::LOGIC_ERROR);

  // merge partial_y
  arrow::Datum incremented_datum(ctx->inputs.front()[0]->column(0));
  for (size_t i = 1; i < ctx->inputs.front().size(); ++i) {
    auto cur_array = ctx->inputs.front()[i]->column(0);
    SERVING_GET_ARROW_RESULT(arrow::compute::Add(incremented_datum, cur_array),
                             incremented_datum);
  }
  auto merged_array = std::static_pointer_cast<arrow::DoubleArray>(
      std::move(incremented_datum).make_array());

  // apply link func
  arrow::DoubleBuilder builder;
  SERVING_CHECK_ARROW_STATUS(builder.Resize(merged_array->length()));
  for (int64_t i = 0; i < merged_array->length(); ++i) {
    auto score =
        ApplyLinkFunc(merged_array->Value(i), link_function_) * yhat_scale_;
    SERVING_CHECK_ARROW_STATUS(builder.Append(score));
  }
  std::shared_ptr<arrow::Array> res_array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&res_array));
  ctx->output =
      MakeRecordBatch(output_schema_, res_array->length(), {res_array});
}

void MergeY::BuildInputSchema() {
  // build input schema
  auto schema =
      arrow::schema({arrow::field(input_col_name_, arrow::float64())});
  input_schema_list_.emplace_back(schema);
}

void MergeY::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(MERGE_Y, MergeY)
REGISTER_OP(MERGE_Y, "0.0.2",
            "Merge all partial y(score) and apply link function")
    .Returnable()
    .Mergeable()
    .DoubleAttr(
        "yhat_scale",
        "In order to prevent value overflow, GLM training is performed on the "
        "scaled y label. So in the prediction process, you need to enlarge "
        "yhat back to get the real predicted value, `yhat = yhat_scale * "
        "link(X * W)`",
        false, true, 1.0)
    .StringAttr(
        "link_function",
        "Type of link function, defined in "
        "`secretflow_serving/protos/link_function.proto`. Optional value: "
        "LF_LOG, LF_LOGIT, LF_INVERSE, "
        "LF_RECIPROCAL, "
        "LF_IDENTITY, LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, "
        "LF_SIGMOID_GA, "
        "LF_SIGMOID_T1, LF_SIGMOID_T3, "
        "LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, "
        "LF_SIGMOID_SEG3, "
        "LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS",
        false, false)
    .StringAttr("input_col_name", "The column name of partial_y", false, false)
    .StringAttr("output_col_name", "The column name of merged score", false,
                false)
    .Input("partial_ys", "The list of partial y, data type: `double`")
    .Output("scores", "The merge result of `partial_ys`, data type: `double`");

}  // namespace secretflow::serving::op
