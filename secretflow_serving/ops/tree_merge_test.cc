// Copyright 2023 Ant Group Co., Ltd.
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

#include "secretflow_serving/ops/tree_merge.h"

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/test_utils.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class TreeMergeTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TreeMergeTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_MERGE",
  "attr_values": {
    "input_col_name": {
      "s": "selects"
    },
    "output_col_name": {
      "s": "weights"
    },
    "leaf_node_ids": {
      "i32s": {
        "data": [
          7, 8, 9, 10, 11, 12, 13, 14
        ]
      }
    },
    "leaf_weights": {
      "ds": {
        "data": [
          -0.116178043, 0.16241236, -0.418656051, -0.0926064253, 0.15993154, 0.358381808, -0.104386188, 0.194736511
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("selects", arrow::binary())};

  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("weights", arrow::float64())});

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

  ComputeContext compute_ctx;
  std::vector<uint8_t> alice_select_0 = {0, /*01100000*/ (1 << 5) | (1 << 6)};
  std::vector<uint8_t> bob_select_0 = {
      0, /*11000011*/ 1 | (1 << 1) | (1 << 6) | (1 << 7)};

  std::vector<uint8_t> alice_select_1 = {0, /*00000101*/ 1 | (1 << 2)};
  std::vector<uint8_t> bob_select_1 = {
      0, /*11001100*/ (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7)};

  {
    std::shared_ptr<arrow::Array> alice_array;
    arrow::BinaryBuilder alice_builder;
    SERVING_CHECK_ARROW_STATUS(
        alice_builder.Append(alice_select_0.data(), alice_select_0.size()));
    SERVING_CHECK_ARROW_STATUS(
        alice_builder.Append(alice_select_1.data(), alice_select_1.size()));
    SERVING_CHECK_ARROW_STATUS(alice_builder.Finish(&alice_array));

    std::shared_ptr<arrow::Array> bob_array;
    arrow::BinaryBuilder bob_builder;
    SERVING_CHECK_ARROW_STATUS(
        bob_builder.Append(bob_select_0.data(), bob_select_0.size()));
    SERVING_CHECK_ARROW_STATUS(
        bob_builder.Append(bob_select_1.data(), bob_select_1.size()));
    SERVING_CHECK_ARROW_STATUS(bob_builder.Finish(&bob_array));

    auto alice_input =
        MakeRecordBatch(arrow::schema(input_fields), 2, {alice_array});
    auto bob_input =
        MakeRecordBatch(arrow::schema(input_fields), 2, {bob_array});

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{alice_input,
                                                         bob_input});
  }

  // expect result
  std::shared_ptr<arrow::Array> weight_array;
  // 01100000 & 11000011 = 01000000
  // 00000101 & 11001100 = 00000100
  SERVING_GET_ARROW_RESULT(
      arrow::ipc::internal::json::ArrayFromJSON(arrow::float64(),
                                                "[-0.104386188, -0.418656051]"),
      weight_array);
  auto expect_result = MakeRecordBatch(expect_output_schema, 2, {weight_array});

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(expect_output_schema));

  std::cout << "expect_select: " << expect_result->ToString() << std::endl;
  std::cout << "result: " << compute_ctx.output->ToString() << std::endl;

  ASSERT_TRUE(compute_ctx.output->Equals(*expect_result));
}

TEST_F(TreeMergeTest, Constructor) {
  // default intercept
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_MERGE",
  "attr_values": {
    "input_col_name": {
      "s": "selects"
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

  auto op_def = OpFactory::GetInstance()->Get("TREE_MERGE");
  OpKernelOptions opts{std::move(node_def), op_def};
  EXPECT_NO_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)));
}

struct Param {
  std::string node_content;
};

class TreeMergeExceptionTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(TreeMergeExceptionTest, Constructor) {
  auto param = GetParam();

  NodeDef node_def;
  JsonToPb(param.node_content, &node_def);

  auto op_def = OpFactory::GetInstance()->Get(node_def.op());
  OpKernelOptions opts{std::move(node_def), op_def};
  EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
               Exception);
}

INSTANTIATE_TEST_SUITE_P(
    TreeMergeExceptionTestSuite, TreeMergeExceptionTest,
    ::testing::Values(
        /*leaf_node_ids and leaf_weights num mismatch*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_MERGE",
  "attr_values": {
    "input_col_name": {
      "s": "selects"
    },
    "output_col_name": {
      "s": "weights"
    },
    "leaf_node_ids": {
      "i32s": {
        "data": [
          7, 8, 9, 10, 11, 12
        ]
      }
    },
    "leaf_weights": {
      "ds": {
        "data": [
          -0.116178043, 0.16241236, -0.418656051, -0.0926064253, 0.15993154, 0.358381808, -0.104386188, 0.194736511
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"},
        /*missing input_col_name*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_MERGE",
  "attr_values": {
    "output_col_name": {
      "s": "weights"
    },
    "leaf_node_ids": {
      "i32s": {
        "data": [
          7, 8, 9, 10, 11, 12, 13, 14
        ]
      }
    },
    "leaf_weights": {
      "ds": {
        "data": [
          -0.116178043, 0.16241236, -0.418656051, -0.0926064253, 0.15993154, 0.358381808, -0.104386188, 0.194736511
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"},
        /*missing output_col_name*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_MERGE",
  "attr_values": {
    "input_col_name": {
      "s": "selects"
    },
    "leaf_node_ids": {
      "i32s": {
        "data": [
          7, 8, 9, 10, 11, 12, 13, 14
        ]
      }
    },
    "leaf_weights": {
      "ds": {
        "data": [
          -0.116178043, 0.16241236, -0.418656051, -0.0926064253, 0.15993154, 0.358381808, -0.104386188, 0.194736511
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"}));

}  // namespace secretflow::serving::op
