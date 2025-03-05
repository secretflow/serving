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

#include "secretflow_serving/ops/he/tree/phe_2p_tree_select.h"

#include <memory>

#include "arrow/compute/api.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/he_mgm.h"

#include "secretflow_serving/protos/data_type.pb.h"

namespace secretflow::serving::op::phe_2p {

namespace {}  // namespace
PheTreeSelect::PheTreeSelect(OpKernelOptions opts)
    : OpKernel(std::move(opts)), p_weight_shard_matrix_(1, 1) {
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

  BuildTreeFromNode(opts_.node_def, *opts_.op_def, feature_name_list_, &tree_);

  auto weight_shard_bytes =
      GetNodeBytesAttr<std::string>(opts_.node_def, "weight_shard");

  p_weight_shard_matrix_ = heu_matrix::PMatrix::LoadFrom(weight_shard_bytes);

  SERVING_ENFORCE_EQ(p_weight_shard_matrix_.cols(), 1);
  if (!tree_.nodes.empty()) {
    SERVING_ENFORCE_EQ(p_weight_shard_matrix_.rows(), tree_.num_leaf);
  }

  // output cols
  select_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "select_col_name");
  weight_shard_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "weight_shard_col_name");

  BuildInputSchema();
  BuildOutputSchema();
}

void PheTreeSelect::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE_EQ(ctx->other_party_ids.size(), 1U);
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  const static heu_phe::Plaintext p_zero{ctx->he_kit_mgm->GetSchemaType(), 0};
  const static heu_phe::Plaintext p_one{ctx->he_kit_mgm->GetSchemaType(), 1};

  auto m_encryptor = ctx->he_kit_mgm->GetLocalMatrixEncryptor();

  heu_matrix::PMatrix p_selects_weight_matrix(
      ctx->inputs.front().front()->num_rows(), p_weight_shard_matrix_.rows());

  ::yacl::Buffer selects_buf;
  ::yacl::Buffer selects_weight_buf;
  if (tree_.nodes.empty()) {
    // empty selects, all weight shards
    for (int i = 0; i < p_selects_weight_matrix.rows(); ++i) {
      for (int j = 0; j < p_selects_weight_matrix.cols(); ++j) {
        p_selects_weight_matrix(i, j) = p_weight_shard_matrix_(j, 0);
      }
    }
  } else {
    auto pred_selects_list = TreePredict(tree_, ctx->inputs.front().front());
    // (selects . weight_shard)
    for (int i = 0; i < p_selects_weight_matrix.rows(); ++i) {
      for (int j = 0; j < p_selects_weight_matrix.cols(); ++j) {
        p_selects_weight_matrix(i, j) =
            pred_selects_list[i].CheckLeafSelected(j)
                ? p_weight_shard_matrix_(j, 0)
                : p_zero;
      }
    }

    if (ctx->requester_id == ctx->self_id) {
      heu_matrix::PMatrix p_selects_matrix(pred_selects_list.size(),
                                           tree_.num_leaf);
      // selects
      for (int i = 0; i < p_selects_matrix.rows(); ++i) {
        for (int j = 0; j < p_selects_matrix.cols(); ++j) {
          p_selects_matrix(i, j) =
              pred_selects_list[i].CheckLeafSelected(j) ? p_one : p_zero;
        }
      }
      selects_buf = m_encryptor->Encrypt(p_selects_matrix).Serialize();
    } else {
      heu_matrix::PMatrix p_u64s_selects_matrix(
          ctx->inputs.front().front()->num_rows(),
          TreePredictSelect::GetSelectsU64VecSize(
              p_weight_shard_matrix_.rows()));
      // u64s selects
      for (int r = 0; r < p_u64s_selects_matrix.rows(); ++r) {
        auto u64s_selects = pred_selects_list[r].ToU64Vec();
        SERVING_ENFORCE_EQ(u64s_selects.size(),
                           static_cast<size_t>(p_u64s_selects_matrix.cols()));
        for (size_t idx = 0; idx < u64s_selects.size(); ++idx) {
          p_u64s_selects_matrix(r, idx) = heu_phe::Plaintext{
              ctx->he_kit_mgm->GetSchemaType(), u64s_selects[idx]};
        }
      }
      selects_buf = m_encryptor->Encrypt(p_u64s_selects_matrix).Serialize();
    }
  }

  // passive party can obtain weight_shard results from the selects when
  // executing the merge operator
  if (ctx->requester_id == ctx->self_id) {
    // (selects . weight_shard) result
    selects_weight_buf =
        m_encryptor->Encrypt(p_selects_weight_matrix).Serialize();
  }

  std::shared_ptr<arrow::Array> selects_array;
  BuildBinaryArray(selects_buf, &selects_array);
  std::shared_ptr<arrow::Array> selects_weight_array;
  BuildBinaryArray(selects_weight_buf, &selects_weight_array);
  // build party id array
  std::shared_ptr<arrow::Array> p_array;
  arrow::StringBuilder p_builder;
  SERVING_CHECK_ARROW_STATUS(p_builder.Append(ctx->self_id));
  SERVING_CHECK_ARROW_STATUS(p_builder.Finish(&p_array));
  ctx->output = MakeRecordBatch(output_schema_, 1,
                                {selects_array, selects_weight_array, p_array});
}

void PheTreeSelect::BuildInputSchema() {
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

void PheTreeSelect::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(select_col_name_, arrow::binary()),
                     arrow::field(weight_shard_col_name_, arrow::binary()),
                     arrow::field("party", arrow::utf8())});
}

REGISTER_OP_KERNEL(PHE_2P_TREE_SELECT, PheTreeSelect)
REGISTER_OP(PHE_2P_TREE_SELECT, "0.0.1",
            "Obtaining the local prediction path information of the decision "
            "tree using input features.")
    .StringAttr("input_feature_names", "List of feature names", true, false)
    .StringAttr("input_feature_types",
                "List of input feature data types. Optional value: DT_UINT8, "
                "DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, "
                "DT_INT64, DT_FLOAT, DT_DOUBLE",
                true, false)
    .StringAttr("select_col_name", "Column name of tree select", false, false)
    .StringAttr("weight_shard_col_name", "Column name of weight shard", false,
                false)
    .Int32Attr("root_node_id", "The id of the root tree node", false, true, 0)
    .Int32Attr("node_ids", "The id list of the tree node", true, false)
    .Int32Attr("lchild_ids",
               "The left child node id list, `-1` means not valid", true, false)
    .Int32Attr("rchild_ids",
               "The right child node id list, `-1` means not valid", true,
               false)
    .Int32Attr("split_feature_idxs",
               "The list of split feature index, `-1` means feature not belong "
               "to party or not valid",
               true, false)
    .Int32Attr("leaf_node_ids", "The leaf node ids list.", true, false)
    .DoubleAttr(
        "split_values",
        "node split value, goes left when less than it. valid when `is_leaf "
        "== false`",
        true, false)
    .BytesAttr("weight_shard",
               "The leaf node's weight shard list.The order must remain "
               "consistent with "
               "the sequence in `leaf_node_ids`.",
               false, false)
    .Input("features", "Input feature table")
    .Output("selects",
            "The local prediction path information of the decision tree.");

}  // namespace secretflow::serving::op::phe_2p
