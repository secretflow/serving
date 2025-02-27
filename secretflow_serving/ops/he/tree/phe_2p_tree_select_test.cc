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

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/ops/he/test_utils.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op::phe_2p {

struct Param {
  bool self_request;
};

class PheTreeSelectTest : public ::testing::TestWithParam<Param> {
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

TEST_P(PheTreeSelectTest, Works) {
  auto param = GetParam();

  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_SELECT",
  "attr_values": {
    "input_feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "input_feature_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_FLOAT", "DT_DOUBLE", "DT_FLOAT", "DT_INT16",
          "DT_UINT16", "DT_INT32", "DT_UINT32", "DT_INT64", "DT_UINT64"
        ]
      }
    },
    "select_col_name": {
      "s": "select"
    },
    "weight_shard_col_name": {
      "s": "weight_shard"
    },
    "root_node_id": {
      "i32": 0
    },
    "node_ids": {
      "i32s": {
        "data": [
          0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        ]
      }
    },
    "lchild_ids": {
      "i32s": {
        "data": [
          1, 3, 5, 7, 9, 11, 13, -1, -1, -1, -1, -1, -1, -1, -1
        ]
      }
    },
    "rchild_ids": {
      "i32s": {
        "data": [
          2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1
        ]
      }
    },
    "leaf_node_ids": {
      "i32s": {
        "data": [
          14, 13, 12, 11, 10, 9, 8, 7
        ]
      }
    },
    "split_feature_idxs": {
      "i32s": {
        "data": [
          3, -1, -1, 2, 2, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1
        ]
      }
    },
    "split_values": {
      "ds": {
        "data": [
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633, 0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto weight_shard_matrix = test::GenRawMatrix(8, 1);
  auto p_weight_shard_matrix =
      test::EncodeMatrix(weight_shard_matrix, he_kit_mgm_->GetEncoder().get());
  {
    auto buf = p_weight_shard_matrix.Serialize();
    AttrValue weight_shard_value;
    *weight_shard_value.mutable_by() =
        std::string(buf.data<char>(), buf.size());
    node_def.mutable_attr_values()->insert(
        {"weight_shard", weight_shard_value});
  }

  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("x1", arrow::float64()),
      arrow::field("x2", arrow::float32()),
      arrow::field("x3", arrow::float64()),
      arrow::field("x4", arrow::float32()),
      arrow::field("x5", arrow::int16()),
      arrow::field("x6", arrow::uint16()),
      arrow::field("x7", arrow::int32()),
      arrow::field("x8", arrow::uint32()),
      arrow::field("x9", arrow::int64()),
      arrow::field("x10", arrow::uint64())};

  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("select", arrow::binary()),
                     arrow::field("weight_shard", arrow::binary()),
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
  compute_ctx.self_id = "alice";
  if (param.self_request) {
    compute_ctx.requester_id = "alice";
    compute_ctx.other_party_ids = {"bob"};
  } else {
    compute_ctx.requester_id = "bob";
    compute_ctx.other_party_ids = {"alice"};
  }

  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  {
    std::shared_ptr<arrow::Array> x1, x2, x3, x4, x5, x6, x7, x8, x9, x10;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float64(), "[-0.01, -0.2]"),
                             x1);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[0.01, 0.1]"),
                             x2);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float64(), "[0.01, -1]"), x3);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[-0.01, -0.2]"),
                             x4);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int16(), "[-1, -1]"), x5);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint16(), "[1, 1]"), x6);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[-1, -1]"), x7);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint32(), "[1, 1]"), x8);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int64(), "[-1, -1]"), x9);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint64(), "[1, 1]"), x10);

    auto features = MakeRecordBatch(arrow::schema(input_fields), 2,
                                    {x1, x2, x3, x4, x5, x6, x7, x8, x9, x10});

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }

  yacl::ElapsedTimer timer;

  kernel->Compute(&compute_ctx);

  std::cout << "compute time: " << timer.CountMs() << "\n";

  // expect result
  const static heu_phe::Plaintext p_zero{
      compute_ctx.he_kit_mgm->GetSchemaType(), 0};
  const static heu_phe::Plaintext p_one{compute_ctx.he_kit_mgm->GetSchemaType(),
                                        1};
  heu_matrix::PMatrix p_expect_selects_matrix(2, 8);
  p_expect_selects_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  /*00000110*/
  p_expect_selects_matrix(0, 1) = p_one;
  p_expect_selects_matrix(0, 2) = p_one;
  /*10100000*/
  p_expect_selects_matrix(1, 5) = p_one;
  p_expect_selects_matrix(1, 7) = p_one;

  heu_matrix::PMatrix p_expect_weight_shard_matrix(2, 8);
  p_expect_weight_shard_matrix.ForEach(
      [](int64_t, int64_t, heu_phe::Plaintext* pt) { *pt = p_zero; });
  /*00000110*/
  p_expect_weight_shard_matrix(0, 1) = p_weight_shard_matrix(1, 0);
  p_expect_weight_shard_matrix(0, 2) = p_weight_shard_matrix(2, 0);
  /*10100000*/
  p_expect_weight_shard_matrix(1, 5) = p_weight_shard_matrix(5, 0);
  p_expect_weight_shard_matrix(1, 7) = p_weight_shard_matrix(7, 0);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  ASSERT_EQ(compute_ctx.output->num_rows(), 1);
  // check select
  auto select_array = compute_ctx.output->column(0);
  auto c_result_select_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(select_array)->Value(0));
  ASSERT_EQ(c_result_select_matrix.rows(), p_expect_selects_matrix.rows());
  auto p_result_select_matrix =
      m_kit_.GetDecryptor()->Decrypt(c_result_select_matrix);
  if (param.self_request) {
    ASSERT_EQ(c_result_select_matrix.cols(), p_expect_selects_matrix.cols());

    std::cout << "select result: " << p_result_select_matrix.ToString()
              << std::endl;

    for (int i = 0; i < p_result_select_matrix.rows(); ++i) {
      for (int j = 0; j < p_result_select_matrix.cols(); ++j) {
        EXPECT_EQ(p_result_select_matrix(i, j), p_expect_selects_matrix(i, j));
      }
    }
  } else {
    // u64s selects matrix
    std::vector<TreePredictSelect> tree_selects_list;
    tree_selects_list.reserve(p_result_select_matrix.rows());
    for (int i = 0; i < p_result_select_matrix.rows(); ++i) {
      std::vector<uint64_t> u64s;
      for (int j = 0; j < p_result_select_matrix.cols(); ++j) {
        u64s.emplace_back(p_result_select_matrix(i, j).GetValue<uint64_t>());
      }
      TreePredictSelect tree_selects(u64s);
      /*00000110*/
      if (i == 0) {
        EXPECT_TRUE(tree_selects.CheckLeafSelected(1));
        EXPECT_TRUE(tree_selects.CheckLeafSelected(2));
      }
      /*10100000*/
      if (i == 1) {
        EXPECT_TRUE(tree_selects.CheckLeafSelected(5));
        EXPECT_TRUE(tree_selects.CheckLeafSelected(7));
      }
    }
  }

  // check weight shard
  auto weight_shard_array = compute_ctx.output->column(1);
  if (param.self_request) {
    auto c_result_weight_shard_matrix = heu_matrix::CMatrix::LoadFrom(
        std::static_pointer_cast<arrow::BinaryArray>(weight_shard_array)
            ->Value(0));
    ASSERT_EQ(c_result_weight_shard_matrix.rows(),
              p_expect_weight_shard_matrix.rows());
    ASSERT_EQ(c_result_weight_shard_matrix.cols(),
              p_expect_weight_shard_matrix.cols());
    auto p_result_weight_shard_matrix =
        m_kit_.GetDecryptor()->Decrypt(c_result_weight_shard_matrix);
    for (int i = 0; i < p_result_weight_shard_matrix.rows(); ++i) {
      for (int j = 0; j < p_result_weight_shard_matrix.cols(); ++j) {
        ASSERT_EQ(p_result_weight_shard_matrix(i, j),
                  p_expect_weight_shard_matrix(i, j));
      }
    }
  } else {
    ASSERT_TRUE(std::static_pointer_cast<arrow::BinaryArray>(weight_shard_array)
                    ->Value(0)
                    .empty());
  }

  // check party
  auto party_array = compute_ctx.output->column(2);
  ASSERT_EQ(std::static_pointer_cast<arrow::StringArray>(party_array)->Value(0),
            "alice");
}

INSTANTIATE_TEST_SUITE_P(PheTreeSelectTestSuite, PheTreeSelectTest,
                         ::testing::Values(Param{true}, Param{false}));

class PheTreeSelectEmptyTest : public ::testing::Test {
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

TEST_F(PheTreeSelectEmptyTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "PHE_2P_TREE_SELECT",
  "attr_values": {
    "input_feature_names": {
      "ss": {}
    },
    "input_feature_types": {
      "ss": {}
    },
    "select_col_name": {
      "s": "select"
    },
    "weight_shard_col_name": {
      "s": "weight_shard"
    },
    "root_node_id": {
      "i32": 0
    },
    "node_ids": {
      "i32s": {}
    },
    "lchild_ids": {
      "i32s": {}
    },
    "rchild_ids": {
      "i32s": {}
    },
    "leaf_node_ids": {
      "i32s": {}
    },
    "split_feature_idxs": {
      "i32s": {}
    },
    "split_values": {
      "ds": {}
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto weight_shard_matrix = test::GenRawMatrix(8, 1);
  auto p_weight_shard_matrix =
      test::EncodeMatrix(weight_shard_matrix, he_kit_mgm_->GetEncoder().get());
  {
    auto buf = p_weight_shard_matrix.Serialize();
    AttrValue weight_shard_value;
    *weight_shard_value.mutable_by() =
        std::string(buf.data<char>(), buf.size());
    node_def.mutable_attr_values()->insert(
        {"weight_shard", weight_shard_value});
  }

  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("x1", arrow::float64())};

  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("select", arrow::binary()),
                     arrow::field("weight_shard", arrow::binary()),
                     arrow::field("party", arrow::utf8())});

  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // build input
  ComputeContext compute_ctx;
  compute_ctx.self_id = "alice";
  compute_ctx.requester_id = "alice";
  compute_ctx.other_party_ids = {"bob"};
  compute_ctx.he_kit_mgm = he_kit_mgm_.get();
  {
    std::shared_ptr<arrow::Array> x1;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float64(), "[-0.01, -0.2]"),
                             x1);
    auto features = MakeRecordBatch(arrow::schema(input_fields), 2, {x1});
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }

  yacl::ElapsedTimer timer;

  kernel->Compute(&compute_ctx);

  std::cout << "compute time: " << timer.CountMs() << "\n";

  // expect result
  const static heu_phe::Plaintext p_zero{
      compute_ctx.he_kit_mgm->GetSchemaType(), 0};
  const static heu_phe::Plaintext p_one{compute_ctx.he_kit_mgm->GetSchemaType(),
                                        1};

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_EQ(compute_ctx.output->num_rows(), 1);
  // check select
  auto select_array = compute_ctx.output->column(0);
  ASSERT_TRUE(std::static_pointer_cast<arrow::BinaryArray>(select_array)
                  ->Value(0)
                  .empty());
  // check weight shard
  auto weight_shard_array = compute_ctx.output->column(1);
  auto c_result_weight_shard_matrix = heu_matrix::CMatrix::LoadFrom(
      std::static_pointer_cast<arrow::BinaryArray>(weight_shard_array)
          ->Value(0));
  ASSERT_EQ(c_result_weight_shard_matrix.rows(), 2);
  ASSERT_EQ(c_result_weight_shard_matrix.cols(), weight_shard_matrix.rows());
  auto p_result_weight_shard_matrix =
      m_kit_.GetDecryptor()->Decrypt(c_result_weight_shard_matrix);
  for (int i = 0; i < p_result_weight_shard_matrix.rows(); ++i) {
    for (int j = 0; j < p_result_weight_shard_matrix.cols(); ++j) {
      ASSERT_EQ(p_result_weight_shard_matrix(i, j),
                p_weight_shard_matrix(j, 0));
    }
  }

  // check party
  auto party_array = compute_ctx.output->column(2);
  ASSERT_EQ(std::static_pointer_cast<arrow::StringArray>(party_array)->Value(0),
            "alice");
}

}  // namespace secretflow::serving::op::phe_2p
