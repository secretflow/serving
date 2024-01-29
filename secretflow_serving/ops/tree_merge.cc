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

#include "secretflow_serving/ops/tree_merge.h"

#include <memory>

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

TreeMerge::TreeMerge(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  input_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "input_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");

  // build leaf tree nodes
  auto leaf_node_ids = GetNodeAttr<std::vector<int32_t>>(
      opts_.node_def, *opts_.op_def, "leaf_node_ids");
  CheckAttrValueDuplicate(leaf_node_ids, "leaf_node_ids");
  auto leaf_weights = GetNodeAttr<std::vector<double>>(
      opts_.node_def, *opts_.op_def, "leaf_weights");
  if (!leaf_weights.empty()) {
    SERVING_ENFORCE_EQ(
        leaf_node_ids.size(), leaf_weights.size(),
        "The length of attr value `leaf_node_ids` `leaf_weights` "
        "should be same.");
    // build bfs weight list
    std::map<int32_t, double> leaf_weight_map;
    for (size_t i = 0; i < leaf_node_ids.size(); ++i) {
      leaf_weight_map.emplace(leaf_node_ids[i], leaf_weights[i]);
    }
    for (const auto& [id, weight] : leaf_weight_map) {
      bfs_weights_.emplace_back(weight);
    }
  }

  BuildInputSchema();
  BuildOutputSchema();
}

void TreeMerge::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() > 1,
                  errors::ErrorCode::LOGIC_ERROR);
  // TODO: support for static detection of whether the execution dp_type
  // matches.
  SERVING_ENFORCE(!bfs_weights_.empty(), errors::ErrorCode::LOGIC_ERROR,
                  "party doesn't have leaf weights, can not get merge result.");

  const auto& selects_array = ctx->inputs.front().front()->column(0);

  arrow::DoubleBuilder res_builder;
  SERVING_CHECK_ARROW_STATUS(res_builder.Resize(selects_array->length()));
  for (int64_t row = 0; row < selects_array->length(); ++row) {
    TreePredictSelect merged_select(
        std::static_pointer_cast<arrow::BinaryArray>(selects_array)
            ->Value(row));
    for (size_t p = 1; p < ctx->inputs.front().size(); ++p) {
      TreePredictSelect partial_select(
          std::static_pointer_cast<arrow::BinaryArray>(
              ctx->inputs.front()[p]->column(0))
              ->Value(row));
      merged_select.Merge(partial_select);
    }
    SERVING_CHECK_ARROW_STATUS(
        res_builder.Append(bfs_weights_[merged_select.GetLeafIndex()]));
  }
  std::shared_ptr<arrow::Array> res_array;
  SERVING_CHECK_ARROW_STATUS(res_builder.Finish(&res_array));
  ctx->output =
      MakeRecordBatch(output_schema_, res_array->length(), {res_array});
}

void TreeMerge::BuildInputSchema() {
  // build input schema
  auto schema = arrow::schema({arrow::field(input_col_name_, arrow::binary())});
  input_schema_list_.emplace_back(schema);
}

void TreeMerge::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(TREE_MERGE, TreeMerge)
REGISTER_OP(TREE_MERGE, "0.0.1", "")
    .Mergeable()
    .StringAttr("input_col_name", "The column name of selects", false, false)
    .StringAttr("output_col_name", "The column name of tree predict score",
                false, false)
    .Int32Attr("leaf_node_ids",
               "The id list of the leaf nodes, If party does not possess "
               "weights, the attr can be omitted.",
               true, true, std::vector<int32_t>())
    .DoubleAttr("leaf_weights",
                "The weight list for leaf node, If party does not possess "
                "weights, the attr can be omitted.",
                true, true, std::vector<double>())
    .Input("selects", "Input tree selects")
    .Output("score", "The prediction result of tree.");

}  // namespace secretflow::serving::op
