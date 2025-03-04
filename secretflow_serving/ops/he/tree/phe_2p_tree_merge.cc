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

#include "secretflow_serving/ops/he/tree/phe_2p_tree_merge.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/ops/tree_utils.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op::phe_2p {

PheTreeMerge::PheTreeMerge(OpKernelOptions opts)
    : OpKernel(std::move(opts)), p_weight_shard_matrix_(1, 1) {
  select_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "select_col_name");
  weight_shard_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "weight_shard_col_name");
  output_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "output_col_name");

  auto weight_shard_bytes =
      GetNodeBytesAttr<std::string>(opts_.node_def, "weight_shard");
  p_weight_shard_matrix_ = heu_matrix::PMatrix::LoadFrom(weight_shard_bytes);

  BuildInputSchema();
  BuildOutputSchema();
}

void PheTreeMerge::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE_EQ(ctx->other_party_ids.size(), 1U);
  SERVING_ENFORCE_EQ(ctx->requester_id, *ctx->other_party_ids.begin());
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE_EQ(ctx->inputs.front().size(), 2U);
  SERVING_ENFORCE(ctx->he_kit_mgm, errors::ErrorCode::LOGIC_ERROR);

  std::shared_ptr<arrow::RecordBatch> self_record_batch;
  std::shared_ptr<arrow::RecordBatch> peer_record_batch;
  for (const auto& r : ctx->inputs.front()) {
    auto party_array = r->column(2);
    if (ctx->self_id ==
        std::static_pointer_cast<arrow::StringArray>(party_array)->Value(0)) {
      self_record_batch = r;
    } else {
      peer_record_batch = r;
    }
  }
  SERVING_ENFORCE(self_record_batch, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(peer_record_batch, errors::ErrorCode::LOGIC_ERROR);

  const heu_phe::Plaintext p_zero{ctx->he_kit_mgm->GetSchemaType(), 0};
  const heu_phe::Plaintext p_one{ctx->he_kit_mgm->GetSchemaType(), 1};
  const heu_phe::Ciphertext c_zero =
      ctx->he_kit_mgm->GetDstEncryptor(ctx->requester_id)->EncryptZero();
  const heu_phe::Ciphertext c_one =
      ctx->he_kit_mgm->GetDstEncryptor(ctx->requester_id)
          ->Encrypt({ctx->he_kit_mgm->GetSchemaType(), 1});
  auto evaluator = ctx->he_kit_mgm->GetDstEvaluator(ctx->requester_id);

  auto c_peer_selects_weight_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(peer_record_batch->column(1))
          ->Value(0));

  auto self_selects_buf =
      std::static_pointer_cast<arrow::StringArray>(self_record_batch->column(0))
          ->Value(0);
  auto peer_selects_buf =
      std::static_pointer_cast<arrow::BinaryArray>(peer_record_batch->column(0))
          ->Value(0);

  heu_matrix::PMatrix p_self_select_weight_matrix(
      c_peer_selects_weight_matrix.rows(), p_weight_shard_matrix_.rows());
  if (!self_selects_buf.empty()) {
    auto c_self_selects_matrix =
        heu_matrix::CMatrix::LoadFrom(self_selects_buf);
    auto p_self_u64s_selects_matrix =
        ctx->he_kit_mgm->GetLocalMatrixDecryptor()->Decrypt(
            c_self_selects_matrix);
    std::vector<TreePredictSelect> self_tree_selects_list;
    self_tree_selects_list.reserve(p_self_u64s_selects_matrix.rows());
    for (int i = 0; i < p_self_u64s_selects_matrix.rows(); ++i) {
      std::vector<uint64_t> u64s;
      for (int j = 0; j < p_self_u64s_selects_matrix.cols(); ++j) {
        u64s.emplace_back(
            p_self_u64s_selects_matrix(i, j).GetValue<uint64_t>());
      }
      self_tree_selects_list.emplace_back(TreePredictSelect{u64s});
    }
    p_self_select_weight_matrix.ForEach(
        [&self_tree_selects_list, &p_zero, this](int64_t r, int64_t c,
                                                 heu_phe::Plaintext* pt) {
          *pt = self_tree_selects_list[r].CheckLeafSelected(c)
                    ? p_weight_shard_matrix_(c, 0)
                    : p_zero;
        },
        false);
  } else {
    // select all
    p_self_select_weight_matrix.ForEach(
        [this](int64_t r, int64_t c, heu_phe::Plaintext* pt) {
          *pt = p_weight_shard_matrix_(c, 0);
        },
        false);
  }

  heu_matrix::CMatrix c_peer_selects_matrix(
      c_peer_selects_weight_matrix.rows(), c_peer_selects_weight_matrix.cols());
  if (!peer_selects_buf.empty()) {
    c_peer_selects_matrix = heu_matrix::CMatrix::LoadFrom(peer_selects_buf);
  } else {
    // select all
    c_peer_selects_matrix.ForEach(
        [&c_one](int64_t, int64_t, heu_phe::Ciphertext* ct) { *ct = c_one; },
        false);
  }

  // compute score
  std::mutex score_compute_mutex;
  heu_matrix::CMatrix c_score_matrix(p_self_select_weight_matrix.rows(), 1);
  c_score_matrix.ForEach(
      [&](int64_t, int64_t, heu_phe::Ciphertext* ct) { *ct = c_zero; }, false);
  p_self_select_weight_matrix.ForEach(
      [&](int64_t r, int64_t c, heu_phe::Plaintext* pt) {
        if (*pt != p_zero) {
          auto c_self_weight_shard = evaluator->Mul(
              p_self_select_weight_matrix(r, c), c_peer_selects_matrix(r, c));
          auto c_weight = evaluator->Add(c_self_weight_shard,
                                         c_peer_selects_weight_matrix(r, c));
          std::lock_guard<std::mutex> lock(score_compute_mutex);
          evaluator->AddInplace(&c_score_matrix(r, 0), c_weight);
        }
      });

  std::shared_ptr<arrow::Array> score_array;
  BuildBinaryArray(c_score_matrix.Serialize(), &score_array);
  ctx->output = MakeRecordBatch(output_schema_, 1, {score_array});
}

void PheTreeMerge::BuildInputSchema() {
  // build input schema
  auto schema =
      arrow::schema({arrow::field(select_col_name_, arrow::binary()),
                     arrow::field(weight_shard_col_name_, arrow::binary()),
                     arrow::field("party", arrow::utf8())});
  input_schema_list_.emplace_back(schema);
}

void PheTreeMerge::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(output_col_name_, arrow::binary())});
}

REGISTER_OP_KERNEL(PHE_2P_TREE_MERGE, PheTreeMerge)
REGISTER_OP(
    PHE_2P_TREE_MERGE, "0.0.1",
    "Merge the `PHE_2P_TREE_SELECT` output from multiple parties to obtain a "
    "unique prediction path and return the tree weight.")
    .Mergeable()
    .StringAttr("select_col_name", "The column name of selects", false, false)
    .StringAttr("weight_shard_col_name", "The column name of weight shard",
                false, false)
    .StringAttr("output_col_name", "The column name of tree predict score",
                false, false)
    .BytesAttr("weight_shard",
               "The leaf node's weight shard list.The order must remain "
               "consistent with "
               "the sequence in `PHE_2P_TREE_SELECT.leaf_node_ids`.",
               false, false)
    .Input("selects", "Input tree selects")
    .Output("tree_weight", "The final weight of the tree.");

}  // namespace secretflow::serving::op::phe_2p
