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

#include "secretflow_serving/ops/phe_linear/phe_2p_dot_product.h"

#include "gtest/gtest.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/ops/phe_linear/test_utils.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

struct Param {
  size_t feature_num;
  bool has_offset;
  bool has_intercept;
  bool self_request;
};

class PheDotProductTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {
    he_kit_mgm_ = std::make_unique<he::HeKitMgm>();
    he_kit_mgm_->InitLocalKit(kit_.GetPublicKey()->Serialize(),
                              kit_.GetSecretKey()->Serialize(), 1e6);
    he_kit_mgm_->InitDstKit("bob", remote_kit_.GetPublicKey()->Serialize());
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

TEST_P(PheDotProductTest, Works) {
  auto param = GetParam();
  size_t row_num = 2;

  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_DOT_PRODUCT",
  "attr_values": {
    "result_col_name": {
      "s": "wxe",
    },
    "rand_number_col_name": {
      "s": "rand",
    }
  },
  "op_version": "0.0.1",
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  std::vector<std::string> feature_names;
  for (size_t i = 0; i < param.feature_num; ++i) {
    feature_names.emplace_back("f_" + std::to_string(i));
  }
  std::vector<std::string> feature_types(feature_names.size(), "DT_DOUBLE");
  auto compute_feature_num = feature_names.size();

  if (!feature_names.empty()) {
    AttrValue feature_names_value;
    feature_names_value.mutable_ss()->mutable_data()->Assign(
        feature_names.begin(), feature_names.end());
    node_def.mutable_attr_values()->insert(
        {"feature_names", feature_names_value});

    AttrValue feature_types_value;
    feature_types_value.mutable_ss()->mutable_data()->Assign(
        feature_types.begin(), feature_types.end());
    node_def.mutable_attr_values()->insert(
        {"feature_types", feature_types_value});
  }
  if (param.has_offset) {
    AttrValue offset_col_name_value;
    offset_col_name_value.set_s(feature_names.back());
    node_def.mutable_attr_values()->insert(
        {"offset_col_name", offset_col_name_value});
    compute_feature_num--;
  }

  auto compute_encoder = he_kit_mgm_->GetEncoder(he::kFeatureScale *
                                                 he_kit_mgm_->GetEncodeScale());
  auto base_encoder = he_kit_mgm_->GetEncoder(he_kit_mgm_->GetEncodeScale());

  // generate weight ciphertext
  Double::Matrix weight_matrix;
  if (compute_feature_num > 0) {
    weight_matrix = test::GenRawMatrix(compute_feature_num, 1, 1);
    AttrValue feature_weights_ciphertext;
    {
      auto p_w_m = test::EncodeMatrix(weight_matrix, base_encoder.get());
      auto c_w_m = m_remote_kit_.GetEncryptor()->Encrypt(p_w_m);
      auto c_w_buf = c_w_m.Serialize();
      feature_weights_ciphertext.set_by(
          std::string(c_w_buf.data<char>(), c_w_buf.size()));
    }
    node_def.mutable_attr_values()->insert(
        {"feature_weights_ciphertext", feature_weights_ciphertext});
  }

  // generate intercept
  double intercept = 0;
  if (param.has_intercept) {
    intercept = 1.5;
    AttrValue intercept_ciphertext;
    {
      auto p_i = base_encoder->Encode(intercept);
      auto c_i = remote_kit_.GetEncryptor()->Encrypt(p_i);
      auto c_i_buf = c_i.Serialize();
      intercept_ciphertext.set_by(
          std::string(c_i_buf.data<char>(), c_i_buf.size()));
    }
    node_def.mutable_attr_values()->insert(
        {"intercept_ciphertext", intercept_ciphertext});
  }

  // build expect schema
  std::vector<std::shared_ptr<arrow::Field>> input_fields;
  for (const auto& n : feature_names) {
    input_fields.emplace_back(arrow::field(n, arrow::float64()));
  }
  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("wxe", arrow::binary()),
                     arrow::field("rand", arrow::binary()),
                     arrow::field("party", arrow::utf8())});

  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);

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
  ASSERT_TRUE(output_schema->Equals(expect_output_schema));

  // build input
  ComputeContext compute_ctx;
  compute_ctx.other_party_ids = {"bob"};
  compute_ctx.self_id = "alice";
  if (param.self_request) {
    compute_ctx.requester_id = "alice";
  } else {
    compute_ctx.requester_id = "bob";
  }
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  Double::Matrix input_m;
  if (compute_feature_num > 0 || param.has_offset) {
    input_m.resize(row_num, feature_names.size());
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    for (int j = 0; j < input_m.cols(); ++j) {
      std::shared_ptr<arrow::Array> array;
      arrow::DoubleBuilder builder;
      for (int i = 0; i < input_m.rows(); ++i) {
        input_m(i, j) = i + 1 + (j + 1) * 0.1;
        SERVING_CHECK_ARROW_STATUS(builder.Append(input_m(i, j)));
      }
      SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
      arrays.emplace_back(array);
    }
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{MakeRecordBatch(
            arrow::schema(input_fields), input_m.rows(), arrays)});
  } else {
    // no feature, no offset, mock input
    std::shared_ptr<arrow::Array> array;
    arrow::DoubleBuilder builder;
    for (size_t i = 0; i < row_num; ++i) {
      SERVING_CHECK_ARROW_STATUS(builder.Append(i));
    }
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{MakeRecordBatch(
            arrow::schema({arrow::field("mock", arrow::float64())}), row_num,
            {array})});
  }

  yacl::ElapsedTimer timer;

  kernel->Compute(&compute_ctx);

  std::cout << "compute time: " << timer.CountMs() << "\n";

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  ASSERT_EQ(compute_ctx.output->num_rows(), 1);

  auto rand_array = compute_ctx.output->GetColumnByName("rand");
  heu_phe::Plaintext p_e;
  if (param.self_request) {
    p_e.Deserialize(
        std::static_pointer_cast<arrow::BinaryArray>(rand_array)->Value(0));
  } else {
    heu_phe::Ciphertext c_e;
    c_e.Deserialize(
        std::static_pointer_cast<arrow::BinaryArray>(rand_array)->Value(0));
    p_e = kit_.GetDecryptor()->Decrypt(c_e);
  }

  std::cout << "plain rand: " << p_e << std::endl;

  // expect result
  heu_phe::Plaintext p_one(kit_.GetSchemaType(), 1);
  heu_phe::Plaintext p_minus_one(kit_.GetSchemaType(), -1);
  auto wxe_array = compute_ctx.output->GetColumnByName("wxe");
  auto c_real_wxe_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(wxe_array)->Value(0));
  auto p_real_wxe_matrix =
      m_remote_kit_.GetDecryptor()->Decrypt(c_real_wxe_matrix);
  if (compute_feature_num > 0) {
    Double::Matrix weight_with_offset_matrix;
    if (param.has_offset) {
      weight_with_offset_matrix.resize(weight_matrix.rows() + 1,
                                       weight_matrix.cols());
      weight_with_offset_matrix << weight_matrix,
          Double::RowVec::Constant(1, 1, 1.0);
    } else {
      weight_with_offset_matrix = weight_matrix;
    }
    Double::ColVec expect_score = input_m * weight_with_offset_matrix;
    expect_score.array() += intercept;
    auto p_expect_score_matrix =
        test::EncodeMatrix(expect_score, compute_encoder.get());

    ASSERT_EQ(p_real_wxe_matrix.rows(), expect_score.rows());
    ASSERT_EQ(p_real_wxe_matrix.cols(), expect_score.cols());
    for (int i = 0; i < p_real_wxe_matrix.rows(); ++i) {
      for (int j = 0; j < p_real_wxe_matrix.cols(); ++j) {
        auto p_expect =
            remote_kit_.GetEvaluator()->Sub(p_expect_score_matrix(i, j), p_e);
        ASSERT_TRUE((p_real_wxe_matrix(i, j) - p_expect) <= p_one &&
                    (p_real_wxe_matrix(i, j) - p_expect) >= p_minus_one);
      }
    }
  } else {
    for (size_t i = 0; i < row_num; ++i) {
      double expect_score = intercept;
      if (param.has_offset) {
        expect_score += input_m(i, 0);
      }
      auto p_expect_score = compute_encoder->Encode(expect_score);
      p_expect_score = remote_kit_.GetEvaluator()->Sub(p_expect_score, p_e);
      ASSERT_TRUE((p_real_wxe_matrix(i, 0) - p_expect_score) <= p_one &&
                  (p_real_wxe_matrix(i, 0) - p_expect_score) >= p_minus_one);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(PheDotProductTestSuite, PheDotProductTest,
                         ::testing::Values(Param{4, true, true, false},
                                           Param{4, false, true, false},
                                           Param{4, true, false, false},
                                           Param{4, false, false, false},
                                           Param{4, true, true, true},
                                           Param{1, true, true, false},
                                           Param{0, false, true, false}));

}  // namespace secretflow::serving::op::phe_2p
