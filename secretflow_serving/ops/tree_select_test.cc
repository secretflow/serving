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

#include "secretflow_serving/ops/tree_select.h"

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class TreeSelectTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TreeSelectTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
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
    "output_col_name": {
      "s": "select"
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
      arrow::schema({arrow::field("select", arrow::binary())});

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
  compute_ctx.requester_id = "alice";
  {
    std::shared_ptr<arrow::Array> x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11;
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
    // redundant column
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint64(), "[23, 15]"), x11);
    input_fields.emplace_back(arrow::field("x11", arrow::uint64()));

    auto features =
        MakeRecordBatch(arrow::schema(input_fields), 2,
                        {x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11});
    auto shuffle_fs = ShuffleRecordBatch(features);
    std::cout << shuffle_fs->ToString() << std::endl;

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{shuffle_fs});
  }

  // expect result
  std::vector<uint8_t> except_select_0 = {0, /*00000110*/ (1 << 1) | (1 << 2)};
  std::vector<uint8_t> except_select_1 = {0, /*10100000*/ (1 << 5) | (1 << 7)};

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  std::shared_ptr<arrow::Array> expect_select_array;
  arrow::BinaryBuilder builder;
  SERVING_CHECK_ARROW_STATUS(
      builder.Append(except_select_0.data(), except_select_0.size()));
  SERVING_CHECK_ARROW_STATUS(
      builder.Append(except_select_1.data(), except_select_0.size()));
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&expect_select_array));

  std::cout << "expect_select: " << expect_select_array->ToString()
            << std::endl;
  std::cout << "result: " << compute_ctx.output->column(0)->ToString()
            << std::endl;

  ASSERT_TRUE(compute_ctx.output->column(0)->Equals(expect_select_array));
}

TEST_F(TreeSelectTest, WorksNoFeature) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
  "attr_values": {
    "input_feature_names": {
      "ss": {
        "data": []
      }
    },
    "input_feature_types": {
      "ss": {
        "data": []
      }
    },
    "output_col_name": {
      "s": "select"
    },
    "node_ids": {
      "i32s": {
        "data": []
      }
    },
    "lchild_ids": {
      "i32s": {
        "data": []
      }
    },
    "rchild_ids": {
      "i32s": {
        "data": []
      }
    },
    "leaf_node_ids": {
      "i32s": {
        "data": []
      }
    },
    "split_feature_idxs": {
      "i32s": {
        "data": []
      }
    },
    "split_values": {
      "ds": {
        "data": []
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto expect_input_schema = arrow::schema({});
  auto expect_output_schema =
      arrow::schema({arrow::field("select", arrow::binary())});

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
  compute_ctx.requester_id = "alice";
  {
    std::vector<std::shared_ptr<arrow::Field>> mock_input_fields = {
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

    auto features = MakeRecordBatch(arrow::schema(mock_input_fields), 2,
                                    {x1, x2, x3, x4, x5, x6, x7, x8, x9, x10});
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }

  // expect result
  std::vector<uint8_t> except_select = {};

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  std::shared_ptr<arrow::Array> expect_select_array;
  arrow::BinaryBuilder builder;
  SERVING_CHECK_ARROW_STATUS(
      builder.Append(except_select.data(), except_select.size()));
  SERVING_CHECK_ARROW_STATUS(
      builder.Append(except_select.data(), except_select.size()));
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&expect_select_array));

  std::cout << "expect_select: " << expect_select_array->ToString()
            << std::endl;
  std::cout << "result: " << compute_ctx.output->column(0)->ToString()
            << std::endl;

  ASSERT_TRUE(compute_ctx.output->column(0)->Equals(expect_select_array));
}

struct Param {
  std::string node_content;
};

class TreeSelectExceptionTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(TreeSelectExceptionTest, Constructor) {
  auto param = GetParam();

  NodeDef node_def;
  JsonToPb(param.node_content, &node_def);

  auto op_def = OpFactory::GetInstance()->Get(node_def.op());
  OpKernelOptions opts{std::move(node_def), op_def};
  EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
               Exception);
}

INSTANTIATE_TEST_SUITE_P(
    TreeSelectExceptionTestSuite, TreeSelectExceptionTest,
    ::testing::Values(/*name and type num mismatch*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
  "attr_values": {
    "input_feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9"
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
    "output_col_name": {
      "s": "select"
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
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633,
          0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"},
                      /*missing input_feature_names*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
  "attr_values": {
    "input_feature_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_FLOAT", "DT_DOUBLE", "DT_FLOAT", "DT_INT16",
          "DT_UINT16", "DT_INT32", "DT_UINT32", "DT_INT64", "DT_UINT64"
        ]
      }
    },
    "output_col_name": {
      "s": "select"
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
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633,
          0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"},
                      /*missing input_feature_types*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
  "attr_values": {
    "input_feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "output_col_name": {
      "s": "select"
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
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633,
          0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
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
  "op": "TREE_SELECT",
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
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633,
          0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"},
                      /*mismatch lchild_ids*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "TREE_SELECT",
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
    "output_col_name": {
      "s": "select"
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
          1, 3, 5, 7, 9, 11, 13, -1, -1, -1, -1, -1, -1
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
          -0.154862225, 0.0, 0.0, -0.208345324, 0.301087976, -0.300848633,
          0.0800122, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        ]
      }
    }
  },
  "op_version": "0.0.1"
}
)JSON"}));

}  // namespace secretflow::serving::op
