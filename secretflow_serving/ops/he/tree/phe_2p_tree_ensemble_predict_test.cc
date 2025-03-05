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

#include "secretflow_serving/ops/he/tree/phe_2p_tree_ensemble_predict.h"

#include "gtest/gtest.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

struct Param {
  std::string algo_func;

  std::vector<double> raw_scores;
};

class TreeEnsemblePredictParamTest : public ::testing::TestWithParam<Param> {
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

TEST_P(TreeEnsemblePredictParamTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "raw_scores"
    },
    "output_col_name": {
      "s": "scores"
    },
    "base_score": {
      "d": 0.2
    }
  }
}
)JSON";

  auto param = GetParam();

  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  {
    AttrValue func_value;
    func_value.set_s(param.algo_func);
    node_def.mutable_attr_values()->insert(
        {"algo_func", std::move(func_value)});
  }

  // expect schema
  auto expect_input_schema =
      arrow::schema({arrow::field("raw_scores", arrow::binary())});
  auto expect_output_schema =
      arrow::schema({arrow::field("scores", arrow::float64())});

  // build node
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().returnable());

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
  auto p_raw_scores = heu_matrix::PMatrix(param.raw_scores.size(), 1);
  for (size_t i = 0; i < param.raw_scores.size(); ++i) {
    p_raw_scores(i, 0) = he_kit_mgm_->GetEncoder()->Encode(param.raw_scores[i]);
  }

  auto c_raw_scores = m_kit_.GetEncryptor()->Encrypt(p_raw_scores);
  std::shared_ptr<arrow::Array> raw_scores_array;
  BuildBinaryArray(c_raw_scores.Serialize(), &raw_scores_array);

  ComputeContext compute_ctx;
  compute_ctx.self_id = "alice";
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  compute_ctx.requester_id = "alice";
  compute_ctx.other_party_ids = {"bob"};
  auto input_record_batch =
      MakeRecordBatch(expect_input_schema, 1, {raw_scores_array});
  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{input_record_batch});

  kernel->Compute(&compute_ctx);

  // expect result
  std::shared_ptr<arrow::Array> expect_array;
  arrow::DoubleBuilder expect_res_builder;
  for (size_t row = 0; row < param.raw_scores.size(); ++row) {
    double score = param.raw_scores[row] + 0.2;
    SERVING_CHECK_ARROW_STATUS(expect_res_builder.Append(
        ApplyLinkFunc(score, ParseLinkFuncType(param.algo_func))));
  }
  SERVING_CHECK_ARROW_STATUS(expect_res_builder.Finish(&expect_array));
  auto expect_res =
      MakeRecordBatch(arrow::schema({arrow::field("scores", arrow::float64())}),
                      expect_array->length(), {expect_array});

  // check output
  ASSERT_TRUE(compute_ctx.output);

  std::cout << "expect_score: " << expect_res->ToString() << std::endl;
  std::cout << "result: " << compute_ctx.output->ToString() << std::endl;

  double epsilon = 1E-6;
  ASSERT_TRUE(compute_ctx.output->ApproxEquals(
      *expect_res, arrow::EqualOptions::Defaults().atol(epsilon)));
}

INSTANTIATE_TEST_SUITE_P(
    TreeEnsemblePredictParamTestSuite, TreeEnsemblePredictParamTest,
    ::testing::Values(Param{"LF_IDENTITY", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_RAW", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_MM1", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_MM3", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_GA", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_T1", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_T3", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_T5", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_T7", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_T9", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_LS7", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_SEG3", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_SEG5", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_DF", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_SR", {-0.0406250022, 0.338384569}},
                      Param{"LF_SIGMOID_SEGLS", {-0.0406250022, 0.338384569}}));

}  // namespace secretflow::serving::op
