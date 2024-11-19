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

#include "secretflow_serving/ops/he/linear/phe_2p_merge_y.h"

#include "gtest/gtest.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/he/test_utils.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

struct Param {
  std::string link_func;
  double yhat_scale;
  int32_t exp_iters = 0;
};

class PheMergeYTest : public ::testing::TestWithParam<Param> {
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

TEST_P(PheMergeYTest, Works) {
  auto param = GetParam();

  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_MERGE_Y",
  "attr_values": {
    "decrypted_y_col_name": {
      "s": "decrypted_y",
    },
    "crypted_y_col_name": {
      "s": "crypted_y",
    },
    "score_col_name": {
      "s": "score"
    },
  },
  "op_version": "0.0.1",
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  {
    AttrValue link_func_value;
    link_func_value.set_s(param.link_func);
    node_def.mutable_attr_values()->insert({"link_function", link_func_value});
  }
  {
    AttrValue scale_value;
    scale_value.set_d(param.yhat_scale);
    node_def.mutable_attr_values()->insert({"yhat_scale", scale_value});
  }
  {
    AttrValue exp_iters_value;
    exp_iters_value.set_i32(param.exp_iters);
    node_def.mutable_attr_values()->insert({"exp_iters", exp_iters_value});
  }

  auto compute_encoder = he_kit_mgm_->GetEncoder(he::kFeatureScale *
                                                 he_kit_mgm_->GetEncodeScale());
  auto base_encoder = he_kit_mgm_->GetEncoder(he_kit_mgm_->GetEncodeScale());

  // build input&output schema
  auto expect_decrypted_schema =
      arrow::schema({arrow::field("decrypted_y", arrow::binary())});
  auto expect_crypted_schema =
      arrow::schema({arrow::field("crypted_y", arrow::binary())});
  auto expect_output_schema =
      arrow::schema({arrow::field("score", arrow::float64())});

  // create kernel
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 2);
  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  ASSERT_TRUE(input_schema_list[0]->Equals(expect_decrypted_schema));
  ASSERT_TRUE(input_schema_list[1]->Equals(expect_crypted_schema));
  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  auto alice_y = test::GenRawMatrix(2, 1, 1);
  auto bob_y = test::GenRawMatrix(2, 1, 2);
  // expect output
  Double::Matrix expect_y = alice_y + bob_y;

  // build input
  auto p_alice_y = test::EncodeMatrix(alice_y, compute_encoder.get());
  auto p_bob_y = test::EncodeMatrix(bob_y, compute_encoder.get());
  auto c_bob_y = m_alice_kit_.GetEncryptor()->Encrypt(p_bob_y);

  ComputeContext compute_ctx;
  compute_ctx.other_party_ids = {"bob"};
  compute_ctx.self_id = "alice";
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();

  // build input record_batch
  // decrypted data
  std::shared_ptr<arrow::Array> alice_p_y_array;
  arrow::BinaryBuilder alice_p_y_builder;
  auto p_alice_y_buf = p_alice_y.Serialize();
  SERVING_CHECK_ARROW_STATUS(alice_p_y_builder.Append(
      p_alice_y_buf.data<uint8_t>(), p_alice_y_buf.size()));
  SERVING_CHECK_ARROW_STATUS(alice_p_y_builder.Finish(&alice_p_y_array));

  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{
          MakeRecordBatch(expect_decrypted_schema, 1, {alice_p_y_array})});

  // crypted data
  std::shared_ptr<arrow::Array> bob_c_y_array;
  arrow::BinaryBuilder bob_c_y_builder;
  auto c_bob_y_buf = c_bob_y.Serialize();
  SERVING_CHECK_ARROW_STATUS(
      bob_c_y_builder.Append(c_bob_y_buf.data<uint8_t>(), c_bob_y_buf.size()));
  SERVING_CHECK_ARROW_STATUS(bob_c_y_builder.Finish(&bob_c_y_array));

  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{
          MakeRecordBatch(expect_crypted_schema, 1, {bob_c_y_array})});

  yacl::ElapsedTimer timer;

  kernel->Compute(&compute_ctx);

  std::cout << "---compute time: " << timer.CountMs() << "\n";

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  ASSERT_EQ(compute_ctx.output->num_rows(), 2);

  std::cout << "compute result: " << compute_ctx.output->ToString()
            << std::endl;

  std::shared_ptr<arrow::Array> expect_score_array;
  arrow::DoubleBuilder expect_score_builder;
  for (int i = 0; i < expect_y.rows(); ++i) {
    SERVING_CHECK_ARROW_STATUS(expect_score_builder.Append(
        ApplyLinkFunc(expect_y(i, 0), ParseLinkFuncType(param.link_func),
                      param.exp_iters) *
        param.yhat_scale));
  }
  SERVING_CHECK_ARROW_STATUS(expect_score_builder.Finish(&expect_score_array));

  std::cout << "expect result: " << expect_score_array->ToString() << std::endl;

  double epsilon = 1E-13;
  ASSERT_TRUE(compute_ctx.output->column(0)->ApproxEquals(
      expect_score_array, arrow::EqualOptions::Defaults().atol(epsilon)));
}

INSTANTIATE_TEST_SUITE_P(
    PheMergeYTestSuite, PheMergeYTest,
    ::testing::Values(
        Param{"LF_EXP", 1.0}, Param{"LF_EXP_TAYLOR", 1.0, 4},
        Param{"LF_RECIPROCAL", 1.1}, Param{"LF_IDENTITY", 1.2},
        Param{"LF_SIGMOID_RAW", 1.3}, Param{"LF_SIGMOID_MM1", 1.4},
        Param{"LF_SIGMOID_MM3", 1.5}, Param{"LF_SIGMOID_GA", 1.6},
        Param{"LF_SIGMOID_T1", 1.7}, Param{"LF_SIGMOID_T3", 1.8},
        Param{"LF_SIGMOID_T5", 1.9}, Param{"LF_SIGMOID_T7", 1.01},
        Param{"LF_SIGMOID_T9", 1.02}, Param{"LF_SIGMOID_LS7", 1.03},
        Param{"LF_SIGMOID_SEG3", 1.04}, Param{"LF_SIGMOID_SEG5", 1.05},
        Param{"LF_SIGMOID_DF", 1.06}, Param{"LF_SIGMOID_SR", 1.07},
        Param{"LF_SIGMOID_SEGLS", 1.08}));

}  // namespace secretflow::serving::op::phe_2p
