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

#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class DotProductTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(DotProductTest, Works) {
  // mock attr
  AttrValue feature_name_value;
  {
    std::vector<std::string> names = {"x1", "x2", "x3", "x4", "x5",
                                      "x6", "x7", "x8", "x9", "x10"};
    feature_name_value.mutable_ss()->mutable_data()->Assign(names.begin(),
                                                            names.end());
  }
  AttrValue feature_weight_value;
  {
    std::vector<double> weights = {-0.32705051172041194, 0.95102386482712309,
                                   1.01145375640758,     1.3493328346102449,
                                   -0.97103250283196174, -0.53749125086392879,
                                   0.92053884353604121,  -0.72217737944554916,
                                   0.14693041881992241,  -0.0707939985283586};
    feature_weight_value.mutable_ds()->mutable_data()->Assign(weights.begin(),
                                                              weights.end());
  }
  AttrValue output_col_name_value;
  output_col_name_value.set_s("score");
  AttrValue intercept_value;
  intercept_value.set_d(1.313201881559211);

  // mock feature values
  std::vector<std::vector<double>> feature_value_list = {
      {93, 0.1}, {18, 0.1}, {17, 0.1}, {20, 0.1}, {76, 0.1},
      {74, 0.1}, {25, 0.1}, {2, 0.1},  {31, 0.1}, {37, 0.1}};

  // expect result
  double expect_score_0 = 1.313201881559211 + 1.01145375640758 * 17 +
                          -0.97103250283196174 * 76 +
                          -0.32705051172041194 * 93 + 1.3493328346102449 * 20 +
                          0.95102386482712309 * 18 + -0.53749125086392879 * 74 +
                          0.92053884353604121 * 25 + -0.72217737944554916 * 2 +
                          0.14693041881992241 * 31 + -0.0707939985283586 * 37;
  double expect_score_1 =
      1.313201881559211 + 1.01145375640758 * 0.1 + -0.97103250283196174 * 0.1 +
      -0.32705051172041194 * 0.1 + 1.3493328346102449 * 0.1 +
      0.95102386482712309 * 0.1 + -0.53749125086392879 * 0.1 +
      0.92053884353604121 * 0.1 + -0.72217737944554916 * 0.1 +
      0.14693041881992241 * 0.1 + -0.0707939985283586 * 0.1;
  double epsilon = 1E-13;

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("DOT_PRODUCT");
  node_def.mutable_attr_values()->insert({"feature_names", feature_name_value});
  node_def.mutable_attr_values()->insert(
      {"feature_weights", feature_weight_value});
  node_def.mutable_attr_values()->insert(
      {"output_col_name", output_col_name_value});
  node_def.mutable_attr_values()->insert({"intercept", intercept_value});
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);

  OpKernelOptions opts{mock_node};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (size_t i = 0; i < input_schema_list.size(); ++i) {
    const auto& input_schema = input_schema_list[i];
    ASSERT_EQ(input_schema, kernel->GetInputSchema(i));
    ASSERT_EQ(input_schema->num_fields(), feature_name_value.ss().data_size());
    for (int j = 0; j < input_schema->num_fields(); ++j) {
      auto field = input_schema->field(j);
      ASSERT_EQ(field->name(), feature_name_value.ss().data(j));
      ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
    }
  }

  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_EQ(output_schema->num_fields(), 1);
  for (int j = 0; j < output_schema->num_fields(); ++j) {
    auto field = output_schema->field(j);
    ASSERT_EQ(field->name(), output_col_name_value.s());
    ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
  }

  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = std::make_shared<OpComputeInputs>();
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    for (size_t i = 0; i < feature_value_list.size(); ++i) {
      arrow::DoubleBuilder builder;
      SERVING_CHECK_ARROW_STATUS(builder.AppendValues(feature_value_list[i]));
      std::shared_ptr<arrow::Array> array;
      SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
      arrays.emplace_back(array);
    }
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));
    compute_ctx.inputs->emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }
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

  ASSERT_TRUE(compute_ctx.output->column(0)->ApproxEquals(
      expect_score_array, arrow::EqualOptions::Defaults().atol(epsilon)));
}

TEST_F(DotProductTest, Constructor) {
  // default intercept
  {
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
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_NO_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)));
  }

  // name and weight num mismatch
  {
    std::string json_content = R"JSON(
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
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
  }

  // missing feature_weights
  {
    std::string json_content = R"JSON(
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
    "output_col_name": {
      "s": "y",
    }
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
  }

  // missing feature_names
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "DOT_PRODUCT",
  "attr_values": {
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
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
  }

  // missing output_col_name
  {
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
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
  }
}

}  // namespace secretflow::serving::op
