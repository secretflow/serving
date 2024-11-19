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

#include "secretflow_serving/ops/he/linear/phe_2p_decrypt_peer_y.h"

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/he/test_utils.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

class PheDecryptPeerYTest : public ::testing::Test {
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

TEST_F(PheDecryptPeerYTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_DECRYPT_PEER_Y",
  "attr_values": {
    "partial_y_col_name": {
      "s": "partial_y",
    },
    "decrypted_col_name": {
      "s": "decrypted_y",
    }
  },
  "op_version": "0.0.1",
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto compute_encoder = he_kit_mgm_->GetEncoder(he::kFeatureScale *
                                                 he_kit_mgm_->GetEncodeScale());
  auto base_encoder = he_kit_mgm_->GetEncoder(he_kit_mgm_->GetEncodeScale());

  // build input&output schema
  auto expect_input_schema =
      arrow::schema({arrow::field("partial_y", arrow::binary())});
  auto expect_output_schema =
      arrow::schema({arrow::field("decrypted_y", arrow::binary())});

  // create kernel
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_FALSE(mock_node->GetOpDef()->tag().mergeable());
  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (size_t i = 0; i < input_schema_list.size(); ++i) {
    const auto& input_schema = input_schema_list[i];
    ASSERT_TRUE(input_schema->Equals(expect_input_schema));
  }
  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  std::cout << "real output schema: " << output_schema->ToString() << std::endl;

  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  // build input
  // generate reduce output
  auto bob_y = test::GenRawMatrix(2, 1, 2);
  auto p_bob_y = test::EncodeMatrix(bob_y, compute_encoder.get());
  auto c_bob_y = m_alice_kit_.GetEncryptor()->Encrypt(p_bob_y);

  ComputeContext compute_ctx;
  compute_ctx.other_party_ids = {"bob"};
  compute_ctx.self_id = "alice";
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();

  // bob y
  std::shared_ptr<arrow::Array> bob_c_y_array;
  arrow::BinaryBuilder bob_c_y_builder;
  auto c_bob_y_buf = c_bob_y.Serialize();
  SERVING_CHECK_ARROW_STATUS(
      bob_c_y_builder.Append(c_bob_y_buf.data<uint8_t>(), c_bob_y_buf.size()));
  SERVING_CHECK_ARROW_STATUS(bob_c_y_builder.Finish(&bob_c_y_array));

  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{
          MakeRecordBatch(expect_input_schema, 1, {bob_c_y_array})});

  kernel->Compute(&compute_ctx);

  // check output schema
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  ASSERT_EQ(compute_ctx.output->num_rows(), 1);

  // check decrypted_ye col
  auto expect_ye = bob_y;
  auto decrypted_ye_col = compute_ctx.output->GetColumnByName("decrypted_y");
  auto decrypted_ye_buf =
      std::static_pointer_cast<arrow::BinaryArray>(decrypted_ye_col)->Value(0);
  auto decrypted_ye_matrix = heu_matrix::PMatrix::LoadFrom(decrypted_ye_buf);
  ASSERT_EQ(decrypted_ye_matrix.rows(), 2);
  for (int i = 0; i < decrypted_ye_matrix.rows(); ++i) {
    std::cout << i << " expect_ye: " << expect_ye(i, 0) << std::endl;
    std::cout << i << " actual_ye: " << decrypted_ye_matrix(i, 0) << std::endl;

    ASSERT_EQ(compute_encoder->Decode<double>(decrypted_ye_matrix(i, 0)),
              expect_ye(i, 0));
  }
}

}  // namespace secretflow::serving::op::phe_2p
