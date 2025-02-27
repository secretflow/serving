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

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/he/test_utils.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/ops/tree_utils.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

class PheTreeMergeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    he_kit_mgm_ = std::make_unique<he::HeKitMgm>();
    he_kit_mgm_->InitLocalKit(bob_kit_.GetPublicKey()->Serialize(),
                              bob_kit_.GetSecretKey()->Serialize(), 1e6);
    he_kit_mgm_->InitDstKit("alice", alice_kit_.GetPublicKey()->Serialize());
  }
  void TearDown() override {}

 protected:
  std::unique_ptr<he::HeKitMgm> he_kit_mgm_;

  heu_phe::HeKit bob_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_bob_kit_ = heu_matrix::HeKit(bob_kit_);
  heu_phe::HeKit alice_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_alice_kit_ = heu_matrix::HeKit(alice_kit_);
};

TEST_F(PheTreeMergeTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_MERGE",
  "attr_values": {
    "select_col_name": {
      "s": "selects"
    },
    "weight_shard_col_name": {
      "s": "weight_shard"
    },
    "output_col_name": {
      "s": "weights"
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto alice_weight_shard_matrix = test::GenRawMatrix(8, 1, 2);
  auto p_alice_weight_shard_matrix = test::EncodeMatrix(
      alice_weight_shard_matrix, he_kit_mgm_->GetEncoder().get());
  std::cout << "alice weight shard: " << p_alice_weight_shard_matrix.ToString()
            << "\n";

  auto bob_weight_shard_matrix = test::GenRawMatrix(8, 1, 1);
  auto p_bob_weight_shard_matrix = test::EncodeMatrix(
      bob_weight_shard_matrix, he_kit_mgm_->GetEncoder().get());
  std::cout << "bob weight shard: " << p_bob_weight_shard_matrix.ToString()
            << "\n";

  {
    auto buf = p_bob_weight_shard_matrix.Serialize();
    AttrValue weight_shard_value;
    *weight_shard_value.mutable_by() =
        std::string(buf.data<char>(), buf.size());
    node_def.mutable_attr_values()->insert(
        {"weight_shard", weight_shard_value});
  }

  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("selects", arrow::binary()),
      arrow::field("weight_shard", arrow::binary()),
      arrow::field("party", arrow::utf8())};

  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("weights", arrow::binary())});

  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().mergeable());

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (const auto& input_schema : input_schema_list) {
    ASSERT_TRUE(input_schema->Equals(expect_input_schema));
  }
  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  // build input
  const static heu_phe::Plaintext p_zero{he_kit_mgm_->GetSchemaType(), 0};
  const static heu_phe::Plaintext p_one{he_kit_mgm_->GetSchemaType(), 1};

  // self:bob peer:alice

  // alice
  heu_matrix::PMatrix p_alice_selects_matrix(2, 8);
  heu_matrix::PMatrix p_alice_selects_weight_matrix(2, 8);
  p_alice_selects_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  p_alice_selects_weight_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  /*00000110*/
  p_alice_selects_matrix(0, 1) = p_one;
  p_alice_selects_matrix(0, 2) = p_one;
  p_alice_selects_weight_matrix(0, 1) = p_alice_weight_shard_matrix(1, 0);
  p_alice_selects_weight_matrix(0, 2) = p_alice_weight_shard_matrix(2, 0);
  /*10100000*/
  p_alice_selects_matrix(1, 5) = p_one;
  p_alice_selects_matrix(1, 7) = p_one;
  p_alice_selects_weight_matrix(1, 5) = p_alice_weight_shard_matrix(5, 0);
  p_alice_selects_weight_matrix(1, 7) = p_alice_weight_shard_matrix(7, 0);

  // bob
  std::vector<TreePredictSelect> bob_selects_list(2);
  bob_selects_list[0].SetLeafs(8);
  bob_selects_list[1].SetLeafs(8);
  heu_matrix::PMatrix p_bob_u64_selects_matrix(
      2, TreePredictSelect::GetSelectsU64VecSize(8));
  /*11000011*/
  bob_selects_list[0].SetLeafSelected(0);
  bob_selects_list[0].SetLeafSelected(1);
  bob_selects_list[0].SetLeafSelected(6);
  bob_selects_list[0].SetLeafSelected(7);
  /*11001100*/
  bob_selects_list[1].SetLeafSelected(2);
  bob_selects_list[1].SetLeafSelected(3);
  bob_selects_list[1].SetLeafSelected(6);
  bob_selects_list[1].SetLeafSelected(7);
  for (int i = 0; i < p_bob_u64_selects_matrix.rows(); ++i) {
    auto u64s_selects = bob_selects_list[i].ToU64Vec();
    SERVING_ENFORCE_EQ(u64s_selects.size(),
                       static_cast<size_t>(p_bob_u64_selects_matrix.cols()));
    for (size_t idx = 0; idx < u64s_selects.size(); ++idx) {
      std::cout << "u64s_selects: " << idx << ":" << u64s_selects[idx]
                << std::endl;
      p_bob_u64_selects_matrix(i, idx) =
          heu_phe::Plaintext{he_kit_mgm_->GetSchemaType(), u64s_selects[idx]};
    }
  }

  ComputeContext compute_ctx;
  compute_ctx.requester_id = "alice";
  compute_ctx.self_id = "bob";
  compute_ctx.other_party_ids = {"alice"};
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  {
    auto alice_encryptor = he_kit_mgm_->GetDstMatrixEncryptor("alice");
    auto bob_encryptor = he_kit_mgm_->GetLocalMatrixEncryptor();

    std::shared_ptr<arrow::Array> alice_selects_array;
    std::shared_ptr<arrow::Array> alice_weights_array;
    std::shared_ptr<arrow::Array> alice_party_array;
    std::shared_ptr<arrow::Array> bob_selects_array;
    std::shared_ptr<arrow::Array> bob_weights_array;
    std::shared_ptr<arrow::Array> bob_party_array;

    auto c_alice_selects_matrix =
        alice_encryptor->Encrypt(p_alice_selects_matrix);
    auto c_bob_selects_matrix =
        bob_encryptor->Encrypt(p_bob_u64_selects_matrix);
    BuildBinaryArray(c_alice_selects_matrix.Serialize(), &alice_selects_array);
    BuildBinaryArray(c_bob_selects_matrix.Serialize(), &bob_selects_array);
    auto c_alice_selects_weight_matrix =
        alice_encryptor->Encrypt(p_alice_selects_weight_matrix);
    BuildBinaryArray(c_alice_selects_weight_matrix.Serialize(),
                     &alice_weights_array);
    BuildBinaryArray({}, &bob_weights_array);

    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"alice\"]"),
                             alice_party_array);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"bob\"]"),
                             bob_party_array);

    auto alice_input = MakeRecordBatch(
        arrow::schema(input_fields), 1,
        {alice_selects_array, alice_weights_array, alice_party_array});
    auto bob_input = MakeRecordBatch(
        arrow::schema(input_fields), 1,
        {bob_selects_array, bob_weights_array, bob_party_array});
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{alice_input,
                                                         bob_input});
  }

  kernel->Compute(&compute_ctx);

  // expect result
  std::shared_ptr<arrow::Array> weight_array;
  // 00000110 & 11000011 = 00000010
  // 10100000 & 11001100 = 10000000
  heu_matrix::PMatrix expect_result_matrix(2, 1);
  expect_result_matrix(0, 0) = alice_kit_.GetEvaluator()->Add(
      p_alice_weight_shard_matrix(1, 0), p_bob_weight_shard_matrix(1, 0));
  expect_result_matrix(1, 0) = alice_kit_.GetEvaluator()->Add(
      p_alice_weight_shard_matrix(7, 0), p_bob_weight_shard_matrix(7, 0));

  std::cout << "expect result: " << expect_result_matrix.ToString()
            << std::endl;

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(expect_output_schema));

  auto result_array = compute_ctx.output->column(0);
  ASSERT_EQ(result_array->length(), 1);
  auto c_result_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(result_array)->Value(0));
  auto p_result_matrix = m_alice_kit_.GetDecryptor()->Decrypt(c_result_matrix);

  std::cout << "result: " << p_result_matrix.ToString() << std::endl;

  ASSERT_EQ(p_result_matrix.rows(), expect_result_matrix.rows());
  ASSERT_EQ(p_result_matrix.cols(), expect_result_matrix.cols());
  for (int i = 0; i < p_result_matrix.rows(); ++i) {
    ASSERT_EQ(p_result_matrix(i, 0), expect_result_matrix(i, 0));
  }
}

struct Param {
  bool features_in_self;
};

class PheTreeMergeParamTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {
    he_kit_mgm_ = std::make_unique<he::HeKitMgm>();
    he_kit_mgm_->InitLocalKit(bob_kit_.GetPublicKey()->Serialize(),
                              bob_kit_.GetSecretKey()->Serialize(), 1e6);
    he_kit_mgm_->InitDstKit("alice", alice_kit_.GetPublicKey()->Serialize());
  }
  void TearDown() override {}

 protected:
  std::unique_ptr<he::HeKitMgm> he_kit_mgm_;

  heu_phe::HeKit bob_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_bob_kit_ = heu_matrix::HeKit(bob_kit_);
  heu_phe::HeKit alice_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_alice_kit_ = heu_matrix::HeKit(alice_kit_);
};

TEST_P(PheTreeMergeParamTest, FeaturesInOneParty) {
  // self: bob, peer: alcie
  auto param = GetParam();

  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_MERGE",
  "attr_values": {
    "select_col_name": {
      "s": "selects"
    },
    "weight_shard_col_name": {
      "s": "weight_shard"
    },
    "output_col_name": {
      "s": "weights"
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto alice_weight_shard_matrix = test::GenRawMatrix(8, 1, 2);
  auto p_alice_weight_shard_matrix = test::EncodeMatrix(
      alice_weight_shard_matrix, he_kit_mgm_->GetEncoder().get());
  auto bob_weight_shard_matrix = test::GenRawMatrix(8, 1, 1);
  auto p_bob_weight_shard_matrix = test::EncodeMatrix(
      bob_weight_shard_matrix, he_kit_mgm_->GetEncoder().get());

  {
    auto buf = p_bob_weight_shard_matrix.Serialize();
    AttrValue weight_shard_value;
    *weight_shard_value.mutable_by() =
        std::string(buf.data<char>(), buf.size());
    node_def.mutable_attr_values()->insert(
        {"weight_shard", weight_shard_value});
  }

  auto mock_node = std::make_shared<Node>(std::move(node_def));

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // build input
  const static heu_phe::Plaintext p_zero{he_kit_mgm_->GetSchemaType(), 0};
  const static heu_phe::Plaintext p_one{he_kit_mgm_->GetSchemaType(), 1};
  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("selects", arrow::binary()),
      arrow::field("weight_shard", arrow::binary()),
      arrow::field("party", arrow::utf8())};

  // alice
  heu_matrix::PMatrix p_alice_selects_matrix(2, 8);
  heu_matrix::PMatrix p_alice_selects_weight_matrix(2, 8);
  p_alice_selects_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  p_alice_selects_weight_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  heu_matrix::PMatrix p_bob_u64_selects_matrix(
      2, TreePredictSelect::GetSelectsU64VecSize(8));
  if (param.features_in_self) {
    // empty alice selects
    std::vector<TreePredictSelect> bob_selects_list(2);
    bob_selects_list[0].SetLeafs(8);
    bob_selects_list[1].SetLeafs(8);
    /*00000010*/
    bob_selects_list[0].SetLeafSelected(1);
    /*10000000*/
    bob_selects_list[1].SetLeafSelected(7);
    for (int i = 0; i < p_bob_u64_selects_matrix.rows(); ++i) {
      auto u64s_selects = bob_selects_list[i].ToU64Vec();
      SERVING_ENFORCE_EQ(u64s_selects.size(),
                         static_cast<size_t>(p_bob_u64_selects_matrix.cols()));
      for (size_t idx = 0; idx < u64s_selects.size(); ++idx) {
        p_bob_u64_selects_matrix(i, idx) =
            heu_phe::Plaintext{he_kit_mgm_->GetSchemaType(), u64s_selects[idx]};
      }
    }
    p_alice_selects_weight_matrix.ForEach(
        [&](int64_t r, int64_t c, heu_phe::Plaintext* pt) {
          *pt = p_alice_weight_shard_matrix(c, 0);
        });
  } else {
    // features in alice
    // empty bob selects
    /*00000010*/
    p_alice_selects_matrix(0, 1) = p_one;
    p_alice_selects_weight_matrix(0, 1) = p_alice_weight_shard_matrix(1, 0);
    /*10000000*/
    p_alice_selects_matrix(1, 7) = p_one;
    p_alice_selects_weight_matrix(1, 7) = p_alice_weight_shard_matrix(7, 0);
  }

  std::cout << "p_alice_selects_weight_matrix "
            << p_alice_selects_weight_matrix.ToString() << std::endl;

  ComputeContext compute_ctx;
  compute_ctx.requester_id = "alice";
  compute_ctx.self_id = "bob";
  compute_ctx.other_party_ids = {"alice"};
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  {
    auto alice_encryptor = m_alice_kit_.GetEncryptor();
    auto bob_encryptor = m_bob_kit_.GetEncryptor();

    std::shared_ptr<arrow::Array> alice_selects_array;
    std::shared_ptr<arrow::Array> alice_weights_array;
    std::shared_ptr<arrow::Array> alice_party_array;
    std::shared_ptr<arrow::Array> bob_selects_array;
    std::shared_ptr<arrow::Array> bob_weights_array;
    std::shared_ptr<arrow::Array> bob_party_array;

    if (param.features_in_self) {
      // empty alice selects
      BuildBinaryArray({}, &alice_selects_array);
      auto c_bob_selects_matrix =
          bob_encryptor->Encrypt(p_bob_u64_selects_matrix);
      BuildBinaryArray(c_bob_selects_matrix.Serialize(), &bob_selects_array);
    } else {
      // empty bob selects
      auto c_alice_selects_matrix =
          alice_encryptor->Encrypt(p_alice_selects_matrix);
      BuildBinaryArray(c_alice_selects_matrix.Serialize(),
                       &alice_selects_array);
      BuildBinaryArray({}, &bob_selects_array);
    }
    auto c_alice_selects_weight_matrix =
        alice_encryptor->Encrypt(p_alice_selects_weight_matrix);
    BuildBinaryArray(c_alice_selects_weight_matrix.Serialize(),
                     &alice_weights_array);
    BuildBinaryArray({}, &bob_weights_array);

    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"alice\"]"),
                             alice_party_array);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"bob\"]"),
                             bob_party_array);

    auto alice_input = MakeRecordBatch(
        arrow::schema(input_fields), 1,
        {alice_selects_array, alice_weights_array, alice_party_array});
    auto bob_input = MakeRecordBatch(
        arrow::schema(input_fields), 1,
        {bob_selects_array, bob_weights_array, bob_party_array});
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{alice_input,
                                                         bob_input});
  }

  kernel->Compute(&compute_ctx);

  // expect result
  std::shared_ptr<arrow::Array> weight_array;
  // 00000010
  // 10000000
  heu_matrix::PMatrix expect_result_matrix(2, 1);
  expect_result_matrix(0, 0) = alice_kit_.GetEvaluator()->Add(
      p_alice_weight_shard_matrix(1, 0), p_bob_weight_shard_matrix(1, 0));
  expect_result_matrix(1, 0) = alice_kit_.GetEvaluator()->Add(
      p_alice_weight_shard_matrix(7, 0), p_bob_weight_shard_matrix(7, 0));

  std::cout << "expect result: " << expect_result_matrix.ToString()
            << std::endl;

  // check output
  auto result_array = compute_ctx.output->column(0);
  auto c_result_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(result_array)->Value(0));
  auto p_result_matrix = m_alice_kit_.GetDecryptor()->Decrypt(c_result_matrix);

  std::cout << "result: " << p_result_matrix.ToString() << std::endl;

  ASSERT_EQ(p_result_matrix.rows(), expect_result_matrix.rows());
  ASSERT_EQ(p_result_matrix.cols(), expect_result_matrix.cols());
  for (int i = 0; i < p_result_matrix.rows(); ++i) {
    ASSERT_EQ(p_result_matrix(i, 0), expect_result_matrix(i, 0));
  }
}

INSTANTIATE_TEST_SUITE_P(PheTreeMergeParamTestSuite, PheTreeMergeParamTest,
                         ::testing::Values(Param{false}, Param{true}));

}  // namespace secretflow::serving::op::phe_2p
