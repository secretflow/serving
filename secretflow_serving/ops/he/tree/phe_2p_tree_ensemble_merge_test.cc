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

#include "gtest/gtest.h"

#include "secretflow_serving/ops/he/test_utils.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class TreeEnsembleMergeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    he_kit_mgm_ = std::make_unique<he::HeKitMgm>();
    he_kit_mgm_->InitLocalKit(kit_.GetPublicKey()->Serialize(),
                              kit_.GetSecretKey()->Serialize(), 1e6);
    he_kit_mgm_->InitDstKit("alice", remote_kit_.GetPublicKey()->Serialize());
  }
  void TearDown() override {}

 protected:
  std::unique_ptr<he::HeKitMgm> he_kit_mgm_;

  heu_phe::HeKit kit_ = heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_kit_ = heu_matrix::HeKit(kit_);
  heu_phe::HeKit remote_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_remote_kit_ = heu_matrix::HeKit(remote_kit_);
};

TEST_F(TreeEnsembleMergeTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_ENSEMBLE_MERGE",
  "attr_values": {
    "input_col_name": {
      "s": "tree_score"
    },
    "output_col_name": {
      "s": "ensemble_score"
    },
    "num_trees": {
      "i32": 3
    }
  }
}
)JSON";

  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  // add mock parents for node
  node_def.add_parents("tree_0");
  node_def.add_parents("tree_1");
  node_def.add_parents("tree_2");

  // expect schema
  auto expect_input_schema =
      arrow::schema({arrow::field("tree_score", arrow::binary())});
  auto expect_output_schema =
      arrow::schema({arrow::field("ensemble_score", arrow::binary())});

  // build node
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().variable_inputs());

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), 3);
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (const auto& input_schema : input_schema_list) {
    ASSERT_TRUE(input_schema->Equals(expect_input_schema));
  }
  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  // build input
  auto tree_0_scores = test::GenRawMatrix(2, 1, 1);
  auto tree_1_scores = test::GenRawMatrix(2, 1, 2);
  auto tree_2_scores = test::GenRawMatrix(2, 1, 3);
  auto p_tree_0_scores =
      test::EncodeMatrix(tree_0_scores, he_kit_mgm_->GetEncoder().get());
  auto p_tree_1_scores =
      test::EncodeMatrix(tree_1_scores, he_kit_mgm_->GetEncoder().get());
  auto p_tree_2_scores =
      test::EncodeMatrix(tree_2_scores, he_kit_mgm_->GetEncoder().get());
  auto c_tree_0_scores = m_remote_kit_.GetEncryptor()->Encrypt(p_tree_0_scores);
  auto c_tree_1_scores = m_remote_kit_.GetEncryptor()->Encrypt(p_tree_1_scores);
  auto c_tree_2_scores = m_remote_kit_.GetEncryptor()->Encrypt(p_tree_2_scores);

  std::shared_ptr<arrow::Array> tree_0_scores_array, tree_1_scores_array,
      tree_2_scores_array;
  BuildBinaryArray(c_tree_0_scores.Serialize(), &tree_0_scores_array);
  BuildBinaryArray(c_tree_1_scores.Serialize(), &tree_1_scores_array);
  BuildBinaryArray(c_tree_2_scores.Serialize(), &tree_2_scores_array);
  auto tree_0_batch = MakeRecordBatch(
      arrow::schema({arrow::field("tree_score", arrow::binary())}),
      tree_0_scores_array->length(), {tree_0_scores_array});
  auto tree_1_batch = MakeRecordBatch(
      arrow::schema({arrow::field("tree_score", arrow::binary())}),
      tree_1_scores_array->length(), {tree_1_scores_array});
  auto tree_2_batch = MakeRecordBatch(
      arrow::schema({arrow::field("tree_score", arrow::binary())}),
      tree_2_scores_array->length(), {tree_2_scores_array});

  ComputeContext compute_ctx;
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  compute_ctx.self_id = "bob";
  compute_ctx.requester_id = "alice";
  compute_ctx.other_party_ids = {"alice"};
  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{tree_0_batch});
  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{tree_1_batch});
  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{tree_2_batch});

  kernel->Compute(&compute_ctx);

  // expect result
  auto expect_matrix = tree_0_scores + tree_1_scores + tree_2_scores;
  auto p_expect_matrix =
      test::EncodeMatrix(expect_matrix, he_kit_mgm_->GetEncoder().get());

  // check output
  ASSERT_TRUE(compute_ctx.output);
  auto c_result_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(
          compute_ctx.output->column(0))
          ->Value(0));
  auto p_result_matrix = m_remote_kit_.GetDecryptor()->Decrypt(c_result_matrix);
  for (int i = 0; i < p_result_matrix.rows(); ++i) {
    for (int j = 0; j < p_result_matrix.cols(); ++j) {
      ASSERT_EQ(p_result_matrix(i, j), p_expect_matrix(i, j));
    }
  }
}

}  // namespace secretflow::serving::op
