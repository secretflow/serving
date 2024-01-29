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

#include "secretflow_serving/ops/tree_ensemble_predict.h"

#include "gtest/gtest.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

struct Param {
  std::string algo_func;

  std::vector<std::vector<double>> tree_weights;
};

class TreeEnsemblePredictParamTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(TreeEnsemblePredictParamTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "output_col_name": {
      "s": "scores"
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

    AttrValue num_trees_value;
    num_trees_value.set_i32(param.tree_weights.size());
    node_def.mutable_attr_values()->insert(
        {"num_trees", std::move(num_trees_value)});
  }

  ComputeContext compute_ctx;
  for (size_t i = 0; i < param.tree_weights.size(); ++i) {
    const auto& weights = param.tree_weights[i];

    // build input values
    std::shared_ptr<arrow::Array> w_array;
    arrow::DoubleBuilder builder;
    SERVING_CHECK_ARROW_STATUS(builder.AppendValues(weights));
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&w_array));

    auto w_record_batch = MakeRecordBatch(
        arrow::schema({arrow::field("weights", arrow::float64())}),
        w_array->length(), {w_array});
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{w_record_batch});

    // add mock parents for node
    node_def.add_parents(std::to_string(i));
  }

  // expect result
  std::shared_ptr<arrow::Array> expect_array;
  arrow::DoubleBuilder expect_res_builder;
  for (size_t row = 0; row < param.tree_weights[0].size(); ++row) {
    double score = param.tree_weights[0][row];
    for (size_t col = 1; col < param.tree_weights.size(); ++col) {
      score += param.tree_weights[col][row];
    }
    SERVING_CHECK_ARROW_STATUS(expect_res_builder.Append(
        ApplyLinkFunc(score, ParseLinkFuncType(param.algo_func))));
  }
  SERVING_CHECK_ARROW_STATUS(expect_res_builder.Finish(&expect_array));
  auto expect_res =
      MakeRecordBatch(arrow::schema({arrow::field("scores", arrow::float64())}),
                      expect_array->length(), {expect_array});

  // build node
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().returnable());
  ASSERT_TRUE(mock_node->GetOpDef()->tag().variable_inputs());

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), param.tree_weights.size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (size_t i = 0; i < input_schema_list.size(); ++i) {
    const auto& input_schema = input_schema_list[i];
    ASSERT_EQ(input_schema, kernel->GetInputSchema(i));
    ASSERT_EQ(input_schema->num_fields(), 1);
    auto field = input_schema->field(0);
    ASSERT_EQ(field->name(), "weights");
    ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
  }

  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_EQ(output_schema->num_fields(), 1);
  for (int j = 0; j < output_schema->num_fields(); ++j) {
    auto field = output_schema->field(j);
    ASSERT_EQ(field->name(), "scores");
    ASSERT_EQ(field->type()->id(), arrow::Type::type::DOUBLE);
  }

  // compute
  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);

  std::cout << "expect_score: " << expect_res->ToString() << std::endl;
  std::cout << "result: " << compute_ctx.output->ToString() << std::endl;

  double epsilon = 1E-13;
  ASSERT_TRUE(compute_ctx.output->ApproxEquals(
      *expect_res, arrow::EqualOptions::Defaults().atol(epsilon)));
}

INSTANTIATE_TEST_SUITE_P(
    TreeEnsemblePredictParamTestSuite, TreeEnsemblePredictParamTest,
    ::testing::Values(Param{"LF_IDENTITY",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_RAW",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_MM1",
                            {{0.339306861, 0.519965351},
                             {-0.418656051, -0.0926064253}}},
                      Param{"LF_SIGMOID_MM3",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_GA",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_T1",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239},
                             {-0.381145447, -0.0979942083}}},
                      Param{"LF_SIGMOID_T3",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_T5",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_T7",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_T9",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_LS7",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_SEG3",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_SEG5",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_DF",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_SR",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}},
                      Param{"LF_SIGMOID_SEGLS",
                            {{-0.0406250022, 0.338384569},
                             {-0.116178043, 0.16241236},
                             {-0.196025193, 0.0978358239}}}));

class TreeEnsemblePredictTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TreeEnsemblePredictTest, Constructor) {
  // default algo func
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "output_col_name": {
      "s": "scores"
    },
    "num_trees": {
      "i32": 3
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_NO_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)));
  }

  // num_trees vs parents mismatch
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "output_col_name": {
      "s": "scores"
    },
    "num_trees": {
      "i32": 2
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
    try {
      OpKernelFactory::GetInstance()->Create(std::move(opts));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }

  // wrong algo func
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "output_col_name": {
      "s": "scores"
    },
    "num_trees": {
      "i32": 3
    },
    "algo_func": {
      "s": "SFSDFDF"
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
    try {
      OpKernelFactory::GetInstance()->Create(std::move(opts));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }

  // missing num_trees
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "output_col_name": {
      "s": "scores"
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
    try {
      OpKernelFactory::GetInstance()->Create(std::move(opts));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }

  // missing input_col_name
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "output_col_name": {
      "s": "scores"
    },
    "num_trees": {
      "i32": 3
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
    try {
      OpKernelFactory::GetInstance()->Create(std::move(opts));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }

  // missing output_col_name
  {
    std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "TREE_ENSEMBLE_PREDICT",
  "attr_values": {
    "input_col_name": {
      "s": "weights"
    },
    "num_trees": {
      "i32": 3
    }
  },
  "parents": [
    "node_1", "node_2", "node_3"
  ]
}
)JSON";

    NodeDef node_def;
    JsonToPb(json_content, &node_def);

    auto op_def = OpFactory::GetInstance()->Get("TREE_ENSEMBLE_PREDICT");
    OpKernelOptions opts{std::move(node_def), op_def};
    EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
                 Exception);
    try {
      OpKernelFactory::GetInstance()->Create(std::move(opts));
    } catch (const std::exception& e) {
      std::cout << e.what() << std::endl;
    }
  }
}

}  // namespace secretflow::serving::op
