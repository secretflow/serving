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

#include "secretflow_serving/ops/tree_ensemble_predict.h"

#include <memory>

#include "arrow/compute/api.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

TreeEnsemblePredict::TreeEnsemblePredict(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  input_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "input_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");
  num_trees_ = GetNodeAttr<int32_t>(opts_.node_def, "num_trees");
  SERVING_ENFORCE_EQ(static_cast<size_t>(num_trees_), num_inputs_,
                     "the number of inputs does not meet the number of trees.");

  auto func_type_str =
      GetNodeAttr<std::string>(opts_.node_def, *opts_.op_def, "algo_func");
  func_type_ = ParseLinkFuncType(func_type_str);

  base_score_ =
      GetNodeAttr<double>(opts_.node_def, *opts_.op_def, "base_score");

  BuildInputSchema();
  BuildOutputSchema();
}

void TreeEnsemblePredict::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE(ctx->inputs.size() == num_inputs_,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  // 日志埋点，打印输入的数据表信息
  SPDLOG_INFO("tree_ensemble_predict input: {}", ctx->inputs.front()[0]->ToString());

  // merge trees weight
  arrow::Datum incremented_datum(ctx->inputs.front().front()->column(0));
  for (size_t i = 1; i < ctx->inputs.size(); ++i) {
    auto cur_array = ctx->inputs[i].front()->column(0);
    SERVING_GET_ARROW_RESULT(arrow::compute::Add(incremented_datum, cur_array),
                             incremented_datum);
  }
  auto merged_array = std::static_pointer_cast<arrow::DoubleArray>(
      std::move(incremented_datum).make_array());

  // apply link func
  arrow::DoubleBuilder builder;
  SERVING_CHECK_ARROW_STATUS(builder.Resize(merged_array->length()));
  for (int64_t i = 0; i < merged_array->length(); ++i) {
    auto score = merged_array->Value(i) + base_score_;
    score = ApplyLinkFunc(score, func_type_);
    SERVING_CHECK_ARROW_STATUS(builder.Append(score));
  }
  std::shared_ptr<arrow::Array> res_array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&res_array));
  ctx->output =
      MakeRecordBatch(output_schema_, res_array->length(), {res_array});
  // 日志埋点，打印输出的数据表信息
  SPDLOG_INFO("tree_ensemble_predict output: {}", ctx->output->ToString());
}

void TreeEnsemblePredict::BuildInputSchema() {
  // build input schema
  auto schema =
      arrow::schema({arrow::field(input_col_name_, arrow::float64())});
  for (size_t i = 0; i < num_inputs_; ++i) {
    input_schema_list_.emplace_back(schema);
  }
}

void TreeEnsemblePredict::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(TREE_ENSEMBLE_PREDICT, TreeEnsemblePredict)
REGISTER_OP(TREE_ENSEMBLE_PREDICT, "0.0.1",
            "Accept the weighted results from multiple trees (`TREE_SELECT` + "
            "`TREE_MERGE`), merge them, and obtain the final prediction result "
            "of the tree ensemble.")
    .VariableInputs()
    .Returnable()
    .StringAttr("input_col_name", "The column name of tree weight", false,
                false)
    .StringAttr("output_col_name",
                "The column name of tree ensemble predict score", false, false)
    .Int32Attr("num_trees", "The number of ensemble's tree", false, false)
    .DoubleAttr("base_score", "The initial prediction score, global bias.",
                false, true, 0.0)
    .StringAttr(
        "algo_func",
        "Optional value: "
        "LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, "
        "LF_SIGMOID_GA, "
        "LF_SIGMOID_T1, LF_SIGMOID_T3, "
        "LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, "
        "LF_SIGMOID_SEG3, "
        "LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS",
        false, true, "LF_IDENTITY")
    .Input("*args", "variable inputs, accept tree's weights")
    .Output("score", "The prediction result of tree ensemble.");

}  // namespace secretflow::serving::op
