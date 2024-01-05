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

#include "secretflow_serving/ops/dot_product.h"

#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/test_utils.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class DotProductTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(DotProductTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_FLOAT", "DT_INT8", "DT_UINT8", "DT_INT16",
          "DT_UINT16", "DT_INT32", "DT_UINT32", "DT_INT64", "DT_UINT64"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "score",
    },
    "intercept": {
      "d": 1.313201881559211
    }
  },
  "op_version": "0.0.2",
}
)JSON";
  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  std::vector<std::shared_ptr<arrow::Field>> input_fields = {
      arrow::field("x1", arrow::float64()),
      arrow::field("x2", arrow::float32()),
      arrow::field("x3", arrow::int8()),
      arrow::field("x4", arrow::uint8()),
      arrow::field("x5", arrow::int16()),
      arrow::field("x6", arrow::uint16()),
      arrow::field("x7", arrow::int32()),
      arrow::field("x8", arrow::uint32()),
      arrow::field("x9", arrow::int64()),
      arrow::field("x10", arrow::uint64())};

  auto expect_input_schema = arrow::schema(input_fields);
  auto expect_output_schema =
      arrow::schema({arrow::field("score", arrow::float64())});

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
  {
    std::shared_ptr<arrow::Array> x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float64(), "[93, -0.1]"), x1);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[18, 0.1]"), x2);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int8(), "[17, -1]"), x3);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint8(), "[20, 1]"), x4);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int16(), "[76, -1]"), x5);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint16(), "[74, 1]"), x6);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[25, -1]"), x7);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint32(), "[2, 1]"), x8);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int64(), "[31, -1]"), x9);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint64(), "[37, 1]"), x10);
    // redundant column
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::uint64(), "[23, 15]"), x11);
    input_fields.emplace_back(arrow::field("x11", arrow::uint64()));

    auto features =
        MakeRecordBatch(arrow::schema(input_fields), 2,
                        {x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11});
    auto shuffle_fs = test::ShuffleRecordBatch(features);
    std::cout << shuffle_fs->ToString() << std::endl;

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{shuffle_fs});
  }

  // expect result
  double expect_score_0 = 1.313201881559211 + -0.32705051172041194 * 93 +
                          0.95102386482712309 * 18 + 1.01145375640758 * 17 +
                          1.3493328346102449 * 20 + -0.97103250283196174 * 76 +
                          -0.53749125086392879 * 74 + 0.92053884353604121 * 25 +
                          -0.72217737944554916 * 2 + 0.14693041881992241 * 31 +
                          -0.0707939985283586 * 37;
  double expect_score_1 = 1.313201881559211 + -0.32705051172041194 * -0.1 +
                          0.95102386482712309 * 0.1 + 1.01145375640758 * -1 +
                          1.3493328346102449 * 1 + -0.97103250283196174 * -1 +
                          -0.53749125086392879 * 1 + 0.92053884353604121 * -1 +
                          -0.72217737944554916 * 1 + 0.14693041881992241 * -1 +
                          -0.0707939985283586 * 1;

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  std::shared_ptr<arrow::Array> expect_score_array;
  arrow::DoubleBuilder builder;
  SERVING_CHECK_ARROW_STATUS(
      builder.AppendValues({expect_score_0, expect_score_1}));
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&expect_score_array));

  std::cout << "expect_score: " << expect_score_array->ToString() << std::endl;
  std::cout << "result: " << compute_ctx.output->column(0)->ToString()
            << std::endl;

  // converting float to double causes the result to lose precision
  // double epsilon = 1E-13;
  double epsilon = 1E-8;
  ASSERT_TRUE(compute_ctx.output->column(0)->ApproxEquals(
      expect_score_array, arrow::EqualOptions::Defaults().atol(epsilon)));
}

TEST_F(DotProductTest, Constructor) {
  // default intercept
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.2",
}
)JSON";

  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  auto op_def = OpFactory::GetInstance()->Get("DOT_PRODUCT");
  OpKernelOptions opts{std::move(node_def), op_def};
  EXPECT_NO_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)));
}

struct Param {
  std::string node_content;
};

class DotProductExceptionTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(DotProductExceptionTest, Constructor) {
  auto param = GetParam();

  NodeDef node_def;
  JsonToPb(param.node_content, &node_def);

  auto op_def = OpFactory::GetInstance()->Get(node_def.op());
  OpKernelOptions opts{std::move(node_def), op_def};
  EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
               Exception);
}

INSTANTIATE_TEST_SUITE_P(
    DotProductExceptionTestSuite, DotProductExceptionTest,
    ::testing::Values(/*name and weight num mismatch*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.2",
}
)JSON"},
                      /*missing feature_weights*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.2",
}
)JSON"},
                      /*missing feature_names*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.2",
}
)JSON"},
                      /*missing output_col_name*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    }
  },
  "op_version": "0.0.2",
}
)JSON"},
                      /*missing feature types*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.2",
}
)JSON"},
                      /*mismatch op version*/ Param{R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
    "feature_names": {
      "ss": {
        "data": [
          "x1", "x2", "x3", "x4", "x5",
          "x6", "x7", "x8", "x9", "x10"
        ]
      }
    },
    "input_types": {
      "ss": {
        "data": [
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE",
          "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE", "DT_DOUBLE"
        ]
      }
    },
    "feature_weights": {
      "ds": {
        "data": [
          -0.32705051172041194, 0.95102386482712309,
          1.01145375640758,     1.3493328346102449,
          -0.97103250283196174, -0.53749125086392879,
          0.92053884353604121,  -0.72217737944554916,
          0.14693041881992241,  -0.0707939985283586
        ]
      }
    },
    "output_col_name": {
      "s": "y",
    }
  },
  "op_version": "0.0.1",
}
)JSON"}));

}  // namespace secretflow::serving::op
