// Copyright 2024 Ant Group Co., Ltd.
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

#include "secretflow_serving/ops/he/tree/phe_2p_tree_ensemble_predict.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op::phe_2p {

PheTreeEnsemblePredict::PheTreeEnsemblePredict(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  input_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "input_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");

  auto func_type_str =
      GetNodeAttr<std::string>(opts_.node_def, *opts_.op_def, "algo_func");
  func_type_ = ParseLinkFuncType(func_type_str);

  base_score_ =
      GetNodeAttr<double>(opts_.node_def, *opts_.op_def, "base_score");

  BuildInputSchema();
  BuildOutputSchema();
}

void PheTreeEnsemblePredict::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE_EQ(ctx->other_party_ids.size(), 1U);
  SERVING_ENFORCE_EQ(ctx->self_id, ctx->requester_id);
  SERVING_ENFORCE_EQ(ctx->inputs.size(), 1U);
  SERVING_ENFORCE_EQ(ctx->inputs.front().size(), 1U);

  // decrypt raw score
  auto c_raw_score_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(
          ctx->inputs.front().front()->column(0))
          ->Value(0));
  auto p_raw_score_matrix =
      ctx->he_kit_mgm->GetLocalMatrixDecryptor()->Decrypt(c_raw_score_matrix);

  // apply link func
  arrow::DoubleBuilder builder;
  SERVING_CHECK_ARROW_STATUS(builder.Resize(p_raw_score_matrix.rows()));
  for (int i = 0; i < p_raw_score_matrix.rows(); ++i) {
    auto score = ctx->he_kit_mgm->GetEncoder()->Decode<double>(
                     p_raw_score_matrix(i, 0)) +
                 base_score_;
    score = ApplyLinkFunc(score, func_type_);
    SERVING_CHECK_ARROW_STATUS(builder.Append(score));
  }
  std::shared_ptr<arrow::Array> res_array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&res_array));
  ctx->output =
      MakeRecordBatch(output_schema_, res_array->length(), {res_array});
}

void PheTreeEnsemblePredict::BuildInputSchema() {
  // build input schema
  auto schema = arrow::schema({arrow::field(input_col_name_, arrow::binary())});
  input_schema_list_.emplace_back(std::move(schema));
}

void PheTreeEnsemblePredict::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(PHE_2P_TREE_ENSEMBLE_PREDICT, PheTreeEnsemblePredict)
REGISTER_OP(PHE_2P_TREE_ENSEMBLE_PREDICT, "0.0.1",
            "Decrypt and calculate the prediction score of the tree ensemble")
    .Returnable()
    .StringAttr(
        "input_col_name",
        "The column name of the tree ensemble encrypted raw predict score",
        false, false)
    .StringAttr("output_col_name",
                "The column name of the ensemble predict score", false, false)
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
    .Input("raw_score", "The encryped raw score of the tree ensemble")
    .Output("score", "The prediction result of the tree ensemble.");

}  // namespace secretflow::serving::op::phe_2p
