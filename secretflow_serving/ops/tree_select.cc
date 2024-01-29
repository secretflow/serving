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

#include "secretflow_serving/ops/tree_select.h"

#include <memory>
#include <set>

#include "arrow/compute/api.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/data_type.pb.h"

namespace secretflow::serving::op {

TreeSelect::TreeSelect(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  // feature name
  feature_name_list_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node_def, "input_feature_names");
  CheckAttrValueDuplicate(feature_name_list_, "input_feature_names");
  // feature types
  feature_type_list_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node_def, "input_feature_types");
  SERVING_ENFORCE_EQ(feature_name_list_.size(), feature_type_list_.size(),
                     "attr:input_feature_names size={} does not match "
                     "attr:input_feature_types size={}, node:{}, op:{}",
                     feature_name_list_.size(), feature_type_list_.size(),
                     opts_.node_def.name(), opts_.node_def.op());
  // output_col_name
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");
  // root node id
  root_node_id_ = GetNodeAttr<int32_t>(opts_.node_def, "root_node_id");

  // build tree nodes
  auto node_ids = GetNodeAttr<std::vector<int32_t>>(opts_.node_def, "node_ids");
  CheckAttrValueDuplicate(node_ids, "node_ids");
  auto lchild_ids =
      GetNodeAttr<std::vector<int32_t>>(opts_.node_def, "lchild_ids");
  CheckAttrValueDuplicate(lchild_ids, "lchild_ids", -1);
  auto rchild_ids =
      GetNodeAttr<std::vector<int32_t>>(opts_.node_def, "rchild_ids");
  CheckAttrValueDuplicate(rchild_ids, "rchild_ids", -1);
  auto leaf_flags =
      GetNodeAttr<std::vector<bool>>(opts_.node_def, "leaf_flags");
  auto split_feature_idx_list =
      GetNodeAttr<std::vector<int32_t>>(opts_.node_def, "split_feature_idxs");
  std::for_each(split_feature_idx_list.begin(), split_feature_idx_list.end(),
                [&](const auto& idx) {
                  if (idx >= 0) {
                    SERVING_ENFORCE_LT(static_cast<size_t>(idx),
                                       feature_name_list_.size());
                    used_feature_idx_list_.emplace(idx);
                  }
                });
  auto split_values =
      GetNodeAttr<std::vector<double>>(opts_.node_def, "split_values");
  SERVING_ENFORCE(node_ids.size() == lchild_ids.size() &&
                      node_ids.size() == rchild_ids.size() &&
                      node_ids.size() == leaf_flags.size() &&
                      node_ids.size() == split_feature_idx_list.size() &&
                      node_ids.size() == split_values.size(),
                  errors::ErrorCode::LOGIC_ERROR,
                  "The length of attr value `node_ids` `lchild_ids` "
                  "`rchild_ids` `leaf_flags` "
                  "`split_feature_idxs` `split_values` "
                  "should be same.");
  for (size_t i = 0; i < node_ids.size(); ++i) {
    TreeNode node{.id = node_ids[i],
                  .lchild_id = lchild_ids[i],
                  .rchild_id = rchild_ids[i],
                  .is_leaf = leaf_flags[i],
                  .split_feature_idx = split_feature_idx_list[i],
                  .split_value = split_values[i]};
    nodes_.emplace(node_ids[i], std::move(node));
  }

  int32_t index = 0;
  for (auto& p : nodes_) {
    if (p.second.is_leaf) {
      ++num_leaf_;
      p.second.leaf_bfs_index = index++;
    }
  }

  BuildInputSchema();
  BuildOutputSchema();
}

void TreeSelect::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  std::map<size_t, std::shared_ptr<arrow::Array>> input_features;
  for (const auto& idx : used_feature_idx_list_) {
    const auto& col = ctx->inputs.front().front()->column(idx);
    if (col->type_id() != arrow::Type::DOUBLE) {
      arrow::Datum double_array_datum;
      SERVING_GET_ARROW_RESULT(
          arrow::compute::Cast(
              col, arrow::compute::CastOptions::Safe(arrow::float64())),
          double_array_datum);
      input_features.emplace(idx, std::move(double_array_datum).make_array());
    } else {
      input_features.emplace(idx, col);
    }
  }

  std::shared_ptr<arrow::Array> res_array;
  arrow::BinaryBuilder builder;
  for (int64_t row = 0; row < ctx->inputs.front().front()->num_rows(); ++row) {
    TreePredictSelect pred_select;
    pred_select.SetLeafs(num_leaf_);
    std::queue<int32_t> nodes;
    nodes.push(root_node_id_);

    while (!nodes.empty()) {
      const auto it = nodes_.find(nodes.front());
      nodes.pop();
      SERVING_ENFORCE(it != nodes_.end(), errors::ErrorCode::LOGIC_ERROR);
      const auto& node = it->second;
      if (node.is_leaf) {
        SERVING_ENFORCE(node.leaf_bfs_index != -1,
                        errors::ErrorCode::LOGIC_ERROR);
        pred_select.SetLeafSelected(node.leaf_bfs_index);
      } else {
        if (node.split_feature_idx < 0) {
          // split feature not belong to this party, both side could be
          // possible
          nodes.push(node.lchild_id);
          nodes.push(node.rchild_id);
        } else {
          auto d_a = std::static_pointer_cast<arrow::DoubleArray>(
              input_features[node.split_feature_idx]);
          SERVING_ENFORCE(d_a, errors::ErrorCode::LOGIC_ERROR);
          if (d_a->Value(row) < node.split_value) {
            nodes.push(node.lchild_id);
          } else {
            nodes.push(node.rchild_id);
          }
        }
      }
    }

    SERVING_CHECK_ARROW_STATUS(
        builder.Append(pred_select.select.data(), pred_select.select.size()));
  }
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&res_array));
  ctx->output = MakeRecordBatch(
      output_schema_, ctx->inputs.front().front()->num_rows(), {res_array});
}

void TreeSelect::BuildInputSchema() {
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

void TreeSelect::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::binary())});
}

REGISTER_OP_KERNEL(TREE_SELECT, TreeSelect)
REGISTER_OP(TREE_SELECT, "0.0.1",
            "Obtaining the local prediction path information of the decision "
            "tree using input features.")
    .StringAttr("input_feature_names", "List of feature names", true, false)
    .StringAttr("input_feature_types",
                "List of input feature data types. Optional value: DT_UINT8, "
                "DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, "
                "DT_INT64, DT_FLOAT, DT_DOUBLE",
                true, false)
    .StringAttr("output_col_name", "Column name of tree select", false, false)
    .Int32Attr("root_node_id", "The id of the root tree node", false, false)
    .Int32Attr("node_ids", "The id list of the tree node", true, false)
    .Int32Attr("lchild_ids",
               "The left child node id list, `-1` means not valid", true, false)
    .Int32Attr("rchild_ids",
               "The right child node id list, `-1` means not valid", true,
               false)
    .BoolAttr("leaf_flags", "The leaf flag list of the nodes", true, false)
    .Int32Attr("split_feature_idxs",
               "The list of split feature index, `-1` means feature not belong "
               "to party or not valid",
               true, false)
    .DoubleAttr(
        "split_values",
        "node split value, goes left when less than it. valid when `is_leaf "
        "== false`",
        true, false)
    .Input("features", "Input feature table")
    .Output("select",
            "The local prediction path information of the decision tree.");

}  // namespace secretflow::serving::op
