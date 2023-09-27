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

#include "secretflow_serving/framework/executor.h"

#include "arrow/compute/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

class ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

namespace op {

class MockOpKernel0 : public OpKernel {
 public:
  explicit MockOpKernel0(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ = {schema};
    output_schema_ = schema;
  }

  // array += 1;
  void Compute(ComputeContext* ctx) override {
    for (size_t i = 0; i < ctx->inputs->size(); ++i) {
      for (size_t j = 0; j < ctx->inputs->at(i).size(); ++j) {
        for (int col_index = 0;
             col_index < ctx->inputs->at(i)[j]->num_columns(); ++col_index) {
          // add index num to every item.
          auto field = ctx->inputs->at(i)[j]->schema()->field(col_index);
          auto array = ctx->inputs->at(i)[j]->column(col_index);
          arrow::Datum incremented_datum(1);
          SERVING_GET_ARROW_RESULT(
              arrow::compute::Add(incremented_datum, array), incremented_datum);
          SERVING_GET_ARROW_RESULT(
              ctx->inputs->at(i)[j]->SetColumn(
                  col_index, field, std::move(incremented_datum).make_array()),
              ctx->inputs->at(i)[j]);
        }
      }
    }
    ctx->output = ctx->inputs->front()[0];
  }

  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel1 : public OpKernel {
 public:
  explicit MockOpKernel1(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ = {schema, schema};
    output_schema_ =
        arrow::schema({arrow::field("test_field_a", arrow::utf8())});
  }

  // input0-array0 + input1-arry1 then cast to string array
  void Compute(ComputeContext* ctx) override {
    SERVING_ENFORCE(ctx->inputs->size() == 2, errors::ErrorCode::LOGIC_ERROR);
    SERVING_ENFORCE(ctx->inputs->front().size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);
    SERVING_ENFORCE(ctx->inputs->at(1).size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);

    auto array_0 = ctx->inputs->front()[0]->column(0);
    auto array_1 = ctx->inputs->at(1)[0]->column(0);

    arrow::Datum incremented_datum;
    SERVING_GET_ARROW_RESULT(arrow::compute::Add(array_0, array_1),
                             incremented_datum);
    auto raw_array = std::move(incremented_datum).make_array();

    arrow::StringBuilder builder;
    for (int i = 0; i < raw_array->length(); ++i) {
      std::shared_ptr<arrow::Scalar> raw_scalar;
      SERVING_GET_ARROW_RESULT(raw_array->GetScalar(i), raw_scalar);
      // cast to string
      std::shared_ptr<arrow::Scalar> scalar;
      SERVING_GET_ARROW_RESULT(raw_scalar->CastTo(arrow::utf8()), scalar);
      SERVING_CHECK_ARROW_STATUS(builder.AppendScalar(*scalar));
    }

    std::shared_ptr<arrow::Array> res_array;
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&res_array));
    ctx->output =
        MakeRecordBatch(output_schema_, res_array->length(), {res_array});
  }
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

REGISTER_OP_KERNEL(TEST_OP_0, MockOpKernel0);
REGISTER_OP_KERNEL(TEST_OP_1, MockOpKernel1);
REGISTER_OP(TEST_OP_0, "0.0.1", "test_desc")
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_1, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input_0", "input_desc")
    .Input("input_1", "input_desc")
    .Output("output", "output_desc");

}  // namespace op

TEST_F(ExecutorTest, Works) {
  std::vector<std::string> node_def_jsons = {
      R"JSON(
{
  "name": "node_a",
  "op": "TEST_OP_0",
}
)JSON",
      R"JSON(
{
  "name": "node_b",
  "op": "TEST_OP_0",
}
)JSON",
      R"JSON(
{
  "name": "node_c",
  "op": "TEST_OP_0",
  "parents": [ "node_a" ],
}
)JSON",
      R"JSON(
{
  "name": "node_d",
  "op": "TEST_OP_1",
  "parents": [ "node_b",  "node_c" ],
}
)JSON"};

  std::string execution_def_json = R"JSON(
{
  "nodes": [
    "node_a", "node_b", "node_c", "node_d"
  ],
  "config": {
    "dispatch_type": "DP_ALL"
  }
}
)JSON";

  // build node
  std::map<std::string, std::shared_ptr<Node>> nodes;
  for (const auto& j : node_def_jsons) {
    NodeDef node_def;
    JsonToPb(j, &node_def);
    auto node = std::make_shared<Node>(std::move(node_def));
    nodes.emplace(node->GetName(), node);
  }
  // build edge
  for (const auto& pair : nodes) {
    const auto& input_nodes = pair.second->GetInputNodeNames();
    for (size_t i = 0; i < input_nodes.size(); ++i) {
      auto n_iter = nodes.find(input_nodes[i]);
      SERVING_ENFORCE(n_iter != nodes.end(), errors::ErrorCode::LOGIC_ERROR);
      auto edge = std::make_shared<Edge>(n_iter->first, pair.first, i);
      n_iter->second->SetOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes));
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  auto input_schema =
      arrow::schema({arrow::field("test_field_0", arrow::float64())});
  std::shared_ptr<arrow::Array> array_0;
  std::shared_ptr<arrow::Array> array_1;
  arrow::DoubleBuilder double_builder;
  SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({1, 2, 3, 4}));
  SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
  double_builder.Reset();
  SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({11, 22, 33, 44}));
  SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_1));
  double_builder.Reset();
  auto input_0 = MakeRecordBatch(input_schema, 4, {array_0});
  auto input_1 = MakeRecordBatch(input_schema, 4, {array_1});

  auto op_inputs_0 = std::make_shared<op::OpComputeInputs>();
  std::vector<std::shared_ptr<arrow::RecordBatch>> r_list_0 = {input_0};
  op_inputs_0->emplace_back(r_list_0);
  auto op_inputs_1 = std::make_shared<op::OpComputeInputs>();
  std::vector<std::shared_ptr<arrow::RecordBatch>> r_list_1 = {input_1};
  op_inputs_1->emplace_back(r_list_1);

  auto inputs = std::make_shared<
      std::map<std::string, std::shared_ptr<op::OpComputeInputs>>>();
  inputs->emplace("node_a", op_inputs_0);
  inputs->emplace("node_b", op_inputs_1);

  // run
  auto output = executor->Run(inputs);

  // build expect
  auto expect_output_schema =
      arrow::schema({arrow::field("test_field_a", arrow::utf8())});
  std::shared_ptr<arrow::Array> expect_array;
  arrow::StringBuilder str_builder;
  SERVING_CHECK_ARROW_STATUS(
      str_builder.AppendValues({"15", "27", "39", "51"}));
  SERVING_CHECK_ARROW_STATUS(str_builder.Finish(&expect_array));

  EXPECT_EQ(output->size(), 1);
  EXPECT_EQ(output->at(0).node_name, "node_d");
  EXPECT_EQ(output->at(0).table->num_columns(), 1);
  EXPECT_TRUE(output->at(0).table->schema()->Equals(expect_output_schema));

  std::cout << output->at(0).table->column(0)->ToString() << std::endl;
  std::cout << expect_array->ToString() << std::endl;

  EXPECT_TRUE(output->at(0).table->column(0)->Equals(expect_array));
}

// TODO: exception case

}  // namespace secretflow::serving
