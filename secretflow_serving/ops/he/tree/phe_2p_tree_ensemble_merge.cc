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

#include "secretflow_serving/ops/he/tree/phe_2p_tree_ensemble_merge.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op::phe_2p {

PheTreeEnsembleMerge::PheTreeEnsembleMerge(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  input_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "input_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");
  num_trees_ = GetNodeAttr<int32_t>(opts_.node_def, "num_trees");
  SERVING_ENFORCE_EQ(static_cast<size_t>(num_trees_), num_inputs_,
                     "the number of inputs does not meet the number of trees.");

  BuildInputSchema();
  BuildOutputSchema();
}

void PheTreeEnsembleMerge::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE_EQ(ctx->other_party_ids.size(), 1U);
  SERVING_ENFORCE_EQ(*ctx->other_party_ids.begin(), ctx->requester_id);
  SERVING_ENFORCE(ctx->inputs.size() == num_inputs_,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  auto evaluator = ctx->he_kit_mgm->GetDstMatrixEvaluator(ctx->requester_id);

  // merge trees result
  auto result_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(
          ctx->inputs.front().front()->column(0))
          ->Value(0));
  for (size_t i = 1; i < ctx->inputs.size(); ++i) {
    auto cur = heu_matrix::CMatrix::LoadFrom(
        std::static_pointer_cast<arrow::BinaryArray>(
            ctx->inputs[i].front()->column(0))
            ->Value(0));
    result_matrix = evaluator->Add(result_matrix, cur);
  }

  std::shared_ptr<arrow::Array> result_array;
  BuildBinaryArray(result_matrix.Serialize(), &result_array);
  ctx->output = MakeRecordBatch(output_schema_, 1, {result_array});
}

void PheTreeEnsembleMerge::BuildInputSchema() {
  // build input schema
  auto schema = arrow::schema({arrow::field(input_col_name_, arrow::binary())});
  input_schema_list_.resize(num_inputs_, schema);
}

void PheTreeEnsembleMerge::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::binary())});
}

REGISTER_OP_KERNEL(PHE_2P_TREE_ENSEMBLE_MERGE, PheTreeEnsembleMerge)
REGISTER_OP(PHE_2P_TREE_ENSEMBLE_MERGE, "0.0.1",
            "Accept the weighted results from multiple trees, then merge them.")
    .VariableInputs()
    .StringAttr("input_col_name", "The column name of tree weight", false,
                false)
    .StringAttr(
        "output_col_name",
        "The column name of the tree ensemble encrypted raw predict score",
        false, false)
    .Int32Attr("num_trees", "The number of ensemble's tree", false, false)
    .Input("*args", "variable inputs, accept tree's weights")
    .Output("merged_raw_score", "The merged raw result of tree ensemble.");

}  // namespace secretflow::serving::op::phe_2p
