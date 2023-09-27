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

#include "secretflow_serving/ops/merge_y.h"

#include "gtest/gtest.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

struct Param {
  std::string link_func;
  double yhat_scale;
};

class MergeYParamTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(MergeYParamTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "input_col_name": {
      "s": "y"
    },
    "output_col_name": {
      "s": "score"
    }
  }
}
)JSON";

  auto param = GetParam();

  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  {
    AttrValue link_func_value;
    link_func_value.set_s(param.link_func);
    node_def.mutable_attr_values()->insert(
        {"link_function", std::move(link_func_value)});
  }
  {
    AttrValue scale_value;
    scale_value.set_d(param.yhat_scale);
    node_def.mutable_attr_values()->insert(
        {"yhat_scale", std::move(scale_value)});
  }

  // mock input values
  std::vector<std::vector<double>> feature_value_list = {
      {0.1, 0.11}, {0.1, 0.12}, {0.1, 0.13}};

  // expect result
  double expect_score_0 =
      ApplyLinkFunc(0.1 + 0.1 + 0.1, param.link_func) * param.yhat_scale;
  double expect_score_1 =
      ApplyLinkFunc(0.11 + 0.12 + 0.13, param.link_func) * param.yhat_scale;
  double epsilon = 1E-13;

  // build node
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().returnable());

  OpKernelOptions opts{mock_node};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (size_t i = 0; i < input_schema_list.size(); ++i) {
    const auto& input_schema = input_schema_list[i];
    ASSERT_EQ(input_schema, kernel->GetInputSchema(i));
    ASSERT_EQ(input_schema->num_fields(), 1);
    auto field = input_schema->field(0);
    ASSERT_EQ(field->name(), "y");
    ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
  }

  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_EQ(output_schema->num_fields(), 1);
  for (int j = 0; j < output_schema->num_fields(); ++j) {
    auto field = output_schema->field(j);
    ASSERT_EQ(field->name(), "score");
    ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
  }

  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = std::make_shared<OpComputeInputs>();
  std::vector<std::shared_ptr<arrow::RecordBatch>> input_list;
  for (size_t i = 0; i < feature_value_list.size(); ++i) {
    arrow::DoubleBuilder builder;
    SERVING_CHECK_ARROW_STATUS(builder.AppendValues(feature_value_list[i]));
    std::shared_ptr<arrow::Array> array;
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
    input_list.emplace_back(
        MakeRecordBatch(input_schema_list.front(), 2, {array}));
  }
  compute_ctx.inputs->emplace_back(std::move(input_list));

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

INSTANTIATE_TEST_SUITE_P(
    MergeYParamTestSuite, MergeYParamTest,
    ::testing::Values(
        Param{"LF_LOG", 1.0}, Param{"LF_LOGIT", 1.0}, Param{"LF_INVERSE", 1.0},
        Param{"LF_LOGIT_V2", 1.0}, Param{"LF_RECIPROCAL", 1.1},
        Param{"LF_INDENTITY", 1.2}, Param{"LF_SIGMOID_RAW", 1.3},
        Param{"LF_SIGMOID_MM1", 1.4}, Param{"LF_SIGMOID_MM3", 1.5},
        Param{"LF_SIGMOID_GA", 1.6}, Param{"LF_SIGMOID_T1", 1.7},
        Param{"LF_SIGMOID_T3", 1.8}, Param{"LF_SIGMOID_T5", 1.9},
        Param{"LF_SIGMOID_T7", 1.01}, Param{"LF_SIGMOID_T9", 1.02},
        Param{"LF_SIGMOID_LS7", 1.03}, Param{"LF_SIGMOID_SEG3", 1.04},
        Param{"LF_SIGMOID_SEG5", 1.05}, Param{"LF_SIGMOID_DF", 1.06},
        Param{"LF_SIGMOID_SR", 1.07}, Param{"LF_SIGMOID_SEGLS", 1.08}));

// TODO: exception case

class MergeYTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(MergeYTest, Constructor) {
  // default intercept
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "link_function": {
      "s": "LF_LOG"
    },
    "input_col_name": {
      "s": "y"
    },
    "output_col_name": {
      "s": "score"
    }
  }
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    OpKernelOptions opts{std::make_shared<Node>(std::move(node_def))};
    EXPECT_NO_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)));
  }

  // wrong link function
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "link_function": {
      "s": "WRONG_FUNC"
    },
    "input_col_name": {
      "s": "y"
    },
    "output_col_name": {
      "s": "score"
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

  // missing link function
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "input_col_name": {
      "s": "y"
    },
    "output_col_name": {
      "s": "score"
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

  // missing input_col_name
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "link_function": {
      "s": "LOG"
    },
    "output_col_name": {
      "s": "score"
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
  "op": "MERGE_Y",
  "attr_values": {
    "link_function": {
      "s": "LOG"
    },
    "input_col_name": {
      "s": "y"
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
