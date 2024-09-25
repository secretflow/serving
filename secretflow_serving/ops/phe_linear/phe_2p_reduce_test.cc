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

#include "secretflow_serving/ops/phe_linear/phe_2p_reduce.h"

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/ops/phe_linear/test_utils.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

struct Param {
  bool select_crypted_for_peer;
};

class PheReduceTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {
    he_kit_mgm_ = std::make_unique<he::HeKitMgm>();
    he_kit_mgm_->InitLocalKit(alice_kit_.GetPublicKey()->Serialize(),
                              alice_kit_.GetSecretKey()->Serialize(), 1e6);
    he_kit_mgm_->InitDstKit("bob", bob_kit_.GetPublicKey()->Serialize());
  }
  void TearDown() override {}

 protected:
  std::unique_ptr<he::HeKitMgm> he_kit_mgm_;

  heu_phe::HeKit alice_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_alice_kit_ = heu_matrix::HeKit(alice_kit_);
  heu_phe::HeKit bob_kit_ =
      heu_phe::HeKit(heu_phe::SchemaType::ZPaillier, 2048);
  heu_matrix::HeKit m_bob_kit_ = heu_matrix::HeKit(bob_kit_);
};

TEST_P(PheReduceTest, Works) {
  auto param = GetParam();

  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_REDUCE",
  "attr_values": {
    "partial_y_col_name": {
      "s": "partial_y",
    },
    "rand_number_col_name": {
      "s": "rand"
    }
  },
  "op_version": "0.0.1",
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  {
    AttrValue select_peer_crypted_value;
    select_peer_crypted_value.set_b(param.select_crypted_for_peer);
    node_def.mutable_attr_values()->insert(
        {"select_crypted_for_peer", select_peer_crypted_value});
  }

  // build input&output schema
  auto expect_input_schema =
      arrow::schema({arrow::field("partial_y", arrow::binary()),
                     arrow::field("rand", arrow::binary()),
                     arrow::field("party", arrow::utf8())});
  auto expect_output_schema =
      arrow::schema({arrow::field("partial_y", arrow::binary())});

  // create kernel
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  ASSERT_TRUE(input_schema_list[0]->Equals(expect_input_schema));
  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  // build input
  uint16_t alice_rand = 11;
  uint16_t bob_rand = 22;
  auto alice_y = test::GenRawMatrix(2, 1, 1);
  auto bob_y = test::GenRawMatrix(2, 1, 2);

  auto compute_encoder = he_kit_mgm_->GetEncoder(he::kFeatureScale *
                                                 he_kit_mgm_->GetEncodeScale());
  auto base_encoder = he_kit_mgm_->GetEncoder(he_kit_mgm_->GetEncodeScale());

  alice_y.array() -= alice_rand;
  bob_y.array() -= bob_rand;
  auto p_alice_y = test::EncodeMatrix(alice_y, compute_encoder.get());
  auto p_bob_y = test::EncodeMatrix(bob_y, compute_encoder.get());
  auto c_alice_y = m_bob_kit_.GetEncryptor()->Encrypt(p_alice_y);
  auto c_bob_y = m_alice_kit_.GetEncryptor()->Encrypt(p_bob_y);
  auto p_alice_rand = compute_encoder->Encode(alice_rand);
  // auto c_alice_rand =
  //     alice_kit_.GetEncryptor()->Encrypt(compute_encoder->Encode(alice_rand));
  auto c_bob_rand =
      bob_kit_.GetEncryptor()->Encrypt(compute_encoder->Encode(bob_rand));

  ComputeContext compute_ctx;
  compute_ctx.other_party_ids = {"bob"};
  compute_ctx.self_id = "alice";
  compute_ctx.requester_id = "alice";
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();

  // build input record_batch
  // decrypted data
  std::shared_ptr<arrow::Array> alice_y_array, bob_y_array, alice_rand_array,
      bob_rand_array, alice_party_array, bob_party_array;
  {
    auto build_y_array_func = [](const heu_matrix::CMatrix& c_m,
                                 std::shared_ptr<arrow::Array>* array) {
      arrow::BinaryBuilder builder;
      auto buf = c_m.Serialize();
      SERVING_CHECK_ARROW_STATUS(
          builder.Append(buf.data<uint8_t>(), buf.size()));
      SERVING_CHECK_ARROW_STATUS(builder.Finish(array));
    };

    auto build_rand_array_func = [](const yacl::Buffer& buf,
                                    std::shared_ptr<arrow::Array>* array) {
      arrow::BinaryBuilder builder;
      SERVING_CHECK_ARROW_STATUS(
          builder.Append(buf.data<uint8_t>(), buf.size()));
      SERVING_CHECK_ARROW_STATUS(builder.Finish(array));
    };

    build_y_array_func(c_alice_y, &alice_y_array);
    build_y_array_func(c_bob_y, &bob_y_array);
    build_rand_array_func(p_alice_rand.Serialize(), &alice_rand_array);
    build_rand_array_func(c_bob_rand.Serialize(), &bob_rand_array);

    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"alice\"]"),
                             alice_party_array);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), "[\"bob\"]"),
                             bob_party_array);
  }

  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{
          MakeRecordBatch(expect_input_schema, 1,
                          {alice_y_array, alice_rand_array, alice_party_array}),
          MakeRecordBatch(expect_input_schema, 1,
                          {bob_y_array, bob_rand_array, bob_party_array})});

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  ASSERT_EQ(compute_ctx.output->num_rows(), 1);

  auto partial_y_array = compute_ctx.output->GetColumnByName("partial_y");
  auto partial_y_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(partial_y_array)->Value(0));
  ASSERT_EQ(partial_y_matrix.rows(), 2);

  if (param.select_crypted_for_peer) {
    for (int i = 0; i < partial_y_matrix.rows(); ++i) {
      auto expect = bob_kit_.GetEvaluator()->Add(c_alice_y(i, 0), c_bob_rand);
      ASSERT_EQ(partial_y_matrix(i, 0), expect);
    }
  } else {
    for (int i = 0; i < partial_y_matrix.rows(); ++i) {
      auto expect = alice_kit_.GetEvaluator()->Add(c_bob_y(i, 0), p_alice_rand);
      ASSERT_EQ(partial_y_matrix(i, 0), expect);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(PheReduceTestSuite, PheReduceTest,
                         ::testing::Values(Param{true}, Param{false}));

}  // namespace secretflow::serving::op::phe_2p
