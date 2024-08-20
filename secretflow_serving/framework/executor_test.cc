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

#include <chrono>
#include <thread>

#include "arrow/compute/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/thread_pool.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

constexpr size_t MASSIVE_NODE_CNT = 1ULL << 14;

class ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ThreadPool::GetInstance()->Start(std::thread::hardware_concurrency());
  }
  void TearDown() override { ThreadPool::GetInstance()->Stop(); }
};

namespace op {

#define REGISTER_TEMPLATE_OP_KERNEL(op_name, class_name, unique_class_id) \
  static KernelRegister<class_name> regist_kernel_##unique_class_id(#op_name);

template <unsigned INPUT_EDGE_COUNT, unsigned ADD_DATUM = 1>
class AddEleOpKernel : public OpKernel {
 public:
  explicit AddEleOpKernel(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ = std::vector(INPUT_EDGE_COUNT, schema);
    output_schema_ = schema;
  }

  // array += 1;
  void DoCompute(ComputeContext* ctx) override {
    for (auto& input : ctx->inputs) {
      for (auto& j : input) {
        for (int col_index = 0; col_index < j->num_columns(); ++col_index) {
          // add index num to every item.
          auto field = j->schema()->field(col_index);
          auto array = j->column(col_index);
          SERVING_ENFORCE(ADD_DATUM != std::numeric_limits<unsigned>::max(),
                          serving::errors::INVALID_ARGUMENT,
                          "add datum: {} is too large", ADD_DATUM);
          arrow::Datum incremented_datum(ADD_DATUM);
          SERVING_GET_ARROW_RESULT(
              arrow::compute::Add(incremented_datum, array), incremented_datum);
          SERVING_GET_ARROW_RESULT(
              j->SetColumn(col_index, field,
                           std::move(incremented_datum).make_array()),
              j);
        }
      }
    }
    ctx->output = ctx->inputs.front()[0];
  }

  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

template <unsigned INPUT_EDGE_COUNT>
class EdgeReduceOpKernel : public OpKernel {
 public:
  explicit EdgeReduceOpKernel(OpKernelOptions opts)
      : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ =
        std::vector<std::shared_ptr<arrow::Schema>>(INPUT_EDGE_COUNT, schema);
    output_schema_ = schema;
  }

  void DoCompute(ComputeContext* ctx) override {
    auto res = ctx->inputs.front();
    for (size_t i = 1; i < ctx->inputs.size(); ++i) {
      for (size_t j = 0; j < ctx->inputs[i].size(); ++j) {
        for (int col_index = 0; col_index < ctx->inputs[i][j]->num_columns();
             ++col_index) {
          auto field = ctx->inputs[i][j]->schema()->field(col_index);
          auto array = ctx->inputs[i][j]->column(col_index);
          arrow::Datum incremented_datum;
          SERVING_GET_ARROW_RESULT(
              arrow::compute::Add(arrow::Datum(res[j]->column(col_index)),
                                  array),
              incremented_datum);
          SERVING_GET_ARROW_RESULT(
              res[j]->SetColumn(col_index, field,
                                std::move(incremented_datum).make_array()),
              res[j]);
        }
      }
    }
    ctx->output = res[0];
  }

  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class AddToStrOpKernel : public OpKernel {
 public:
  explicit AddToStrOpKernel(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ = {schema, schema};
    output_schema_ =
        arrow::schema({arrow::field("test_field_a", arrow::utf8())});
  }

  // input0-array0 + input1-arry1 then cast to string array
  void DoCompute(ComputeContext* ctx) override {
    SERVING_ENFORCE(ctx->inputs.size() == 2, errors::ErrorCode::LOGIC_ERROR);
    SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);
    SERVING_ENFORCE(ctx->inputs[1].size() == 1, errors::ErrorCode::LOGIC_ERROR);

    auto array_0 = ctx->inputs.front()[0]->column(0);
    auto array_1 = ctx->inputs[1][0]->column(0);

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
using AddEleOpKernelOneEdgeAddMax =
    AddEleOpKernel<1, std::numeric_limits<unsigned>::max()>;

REGISTER_TEMPLATE_OP_KERNEL(TEST_OP_ONE_EDGE_ADD_ONE, AddEleOpKernel<1>,
                            AddEleOpKernel_1_1);
REGISTER_TEMPLATE_OP_KERNEL(TEST_OP_ONE_EDGE_ADD_MAX,
                            AddEleOpKernelOneEdgeAddMax,
                            AddEleOpKernel_1_MAX_Exception);
REGISTER_OP_KERNEL(TEST_OP_TWO_EDGE_ADD_TO_STR, AddToStrOpKernel);
REGISTER_TEMPLATE_OP_KERNEL(TEST_OP_REDUCE_MASSIVE_CNT,
                            EdgeReduceOpKernel<MASSIVE_NODE_CNT>,
                            EdgeReduceOpKernel_MASSIVE_NODE_CNT);
REGISTER_TEMPLATE_OP_KERNEL(TEST_OP_REDUCE_2, EdgeReduceOpKernel<2>,
                            EdgeReduceOpKernel_2);
REGISTER_TEMPLATE_OP_KERNEL(TEST_OP_REDUCE_COMPLEX_MASSIVE_CNT,
                            EdgeReduceOpKernel<MASSIVE_NODE_CNT * 3 / 2>,
                            EdgeReduceOpKernel_COMPLEX_MASSIVE_CNT);
REGISTER_OP(TEST_OP_ONE_EDGE_ADD_ONE, "0.0.1", "test_desc")
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_ONE_EDGE_ADD_MAX, "0.0.1", "test_desc")
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_TWO_EDGE_ADD_TO_STR, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input_0", "input_desc")
    .Input("input_1", "input_desc")
    .Output("output", "output_desc");

REGISTER_OP(TEST_OP_REDUCE_MASSIVE_CNT, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .InputList("input", MASSIVE_NODE_CNT, "input_desc")
    .Output("output", "output_desc");

REGISTER_OP(TEST_OP_REDUCE_2, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .InputList("input", 2, "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_REDUCE_COMPLEX_MASSIVE_CNT, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .InputList("input", MASSIVE_NODE_CNT * 3 / 2, "input_desc")
    .Output("output", "output_desc");

}  // namespace op

std::string MakeNodeDefJson(const std::string& name, const std::string& op_name,
                            const std::vector<std::string>& parents = {}) {
  std::string ret =
      R"( { "name": ")" + name + R"(", "op": ")" + op_name + R"(", )";
  if (!parents.empty()) {
    ret += R"("parents": [ )";
    for (const auto& p : parents) {
      ret += '"' + p + '"' + ',';
    }
    ret.back() = ']';
  }
  ret += "}";
  return ret;
}

TEST_F(ExecutorTest, MassiveWorks) {
  std::vector<std::string> node_def_jsons;
  std::string node_list_str;
  node_def_jsons.emplace_back(
      MakeNodeDefJson("node_a", "TEST_OP_ONE_EDGE_ADD_ONE"));
  node_list_str += R"("node_a",)";

  std::vector<std::string> last_level_parents;
  for (auto i = 0; i != MASSIVE_NODE_CNT; ++i) {
    std::string node_name = "node_b_" + std::to_string(i);
    node_list_str += '"' + node_name + '"' + ',';

    node_def_jsons.emplace_back(
        MakeNodeDefJson(node_name, "TEST_OP_ONE_EDGE_ADD_ONE", {"node_a"}));
    last_level_parents.emplace_back(std::move(node_name));
  }
  node_def_jsons.emplace_back(MakeNodeDefJson(
      "node_c", "TEST_OP_REDUCE_MASSIVE_CNT", last_level_parents));
  node_list_str += R"("node_c")";

  std::string execution_def_json =
      R"({"nodes": [)" + node_list_str +
      R"(],"config": {"dispatch_type": "DP_ALL"} })";

  // build node
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes), true, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  std::shared_ptr<arrow::RecordBatch> inputs;
  {
    auto input_schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    std::shared_ptr<arrow::Array> array_0;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({1, 2, 3, 4}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
    double_builder.Reset();
    inputs = MakeRecordBatch(input_schema, 4, {array_0});
  }
  // run
  auto output = executor->Run(inputs);

  // build expect
  auto expect_output_schema =
      arrow::schema({arrow::field("test_field_0", arrow::float64())});
  std::shared_ptr<arrow::Array> expect_array;
  arrow::DoubleBuilder array_builder;
  SERVING_CHECK_ARROW_STATUS(
      array_builder.AppendValues({3 * MASSIVE_NODE_CNT, 4 * MASSIVE_NODE_CNT,
                                  5 * MASSIVE_NODE_CNT, 6 * MASSIVE_NODE_CNT}));
  SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&expect_array));

  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.at(0).node_name, "node_c");
  EXPECT_EQ(output.at(0).table->num_columns(), 1);
  EXPECT_TRUE(output.at(0).table->schema()->Equals(expect_output_schema));

  std::cout << output.at(0).table->column(0)->ToString() << std::endl;
  std::cout << expect_array->ToString() << std::endl;

  EXPECT_TRUE(output.at(0).table->column(0)->Equals(expect_array));
}

TEST_F(ExecutorTest, ComplexMassiveWorks) {
  std::vector<std::string> node_def_jsons;
  std::string node_list_str;
  node_def_jsons.emplace_back(
      MakeNodeDefJson("node_a", "TEST_OP_ONE_EDGE_ADD_ONE"));
  node_list_str += R"("node_a",)";

  std::vector<std::string> last_level_parents;
  for (auto i = 0; i != MASSIVE_NODE_CNT; ++i) {
    std::string node_name = "node_b_" + std::to_string(i);
    node_list_str += '"' + node_name + '"' + ',';

    node_def_jsons.emplace_back(
        MakeNodeDefJson(node_name, "TEST_OP_ONE_EDGE_ADD_ONE", {"node_a"}));
    last_level_parents.emplace_back(std::move(node_name));
  }

  unsigned node_c_count = MASSIVE_NODE_CNT / 2;
  for (unsigned i = 0; i != node_c_count; ++i) {
    std::string node_name = "node_c_" + std::to_string(i);
    node_list_str += '"' + node_name + '"' + ',';

    node_def_jsons.emplace_back(
        MakeNodeDefJson(node_name, "TEST_OP_REDUCE_2",
                        {"node_b_" + std::to_string(i * 2),
                         "node_b_" + std::to_string(i * 2 + 1)}));
    last_level_parents.emplace_back(std::move(node_name));
  }

  node_def_jsons.emplace_back(MakeNodeDefJson(
      "node_d", "TEST_OP_REDUCE_COMPLEX_MASSIVE_CNT", last_level_parents));
  node_list_str += R"("node_d")";

  std::string execution_def_json =
      R"({"nodes": [)" + node_list_str +
      R"(],"config": {"dispatch_type": "DP_ALL"} })";

  // build node
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes), true, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  std::shared_ptr<arrow::RecordBatch> inputs;
  {
    auto input_schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    std::shared_ptr<arrow::Array> array_0;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({1, 2, 3, 4}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
    double_builder.Reset();
    inputs = MakeRecordBatch(input_schema, 4, {array_0});
  }

  // run
  auto output = executor->Run(inputs);

  // build expect
  auto expect_output_schema =
      arrow::schema({arrow::field("test_field_0", arrow::float64())});
  std::shared_ptr<arrow::Array> expect_array;
  arrow::DoubleBuilder array_builder;
  SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
      {3 * MASSIVE_NODE_CNT * 2, 4 * MASSIVE_NODE_CNT * 2,
       5 * MASSIVE_NODE_CNT * 2, 6 * MASSIVE_NODE_CNT * 2}));
  SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&expect_array));

  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.at(0).node_name, "node_d");
  EXPECT_EQ(output.at(0).table->num_columns(), 1);
  EXPECT_TRUE(output.at(0).table->schema()->Equals(expect_output_schema));

  std::cout << output.at(0).table->column(0)->ToString() << std::endl;
  std::cout << expect_array->ToString() << std::endl;

  EXPECT_TRUE(output.at(0).table->column(0)->Equals(expect_array));
}

TEST_F(ExecutorTest, FeatureInput) {
  std::vector<std::string> node_def_jsons = {
      R"JSON(
{
  "name": "node_a",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_b",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_c",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
  "parents": [ "node_a" ],
}
)JSON",
      R"JSON(
{
  "name": "node_d",
  "op": "TEST_OP_TWO_EDGE_ADD_TO_STR",
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
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes), true, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  std::shared_ptr<arrow::RecordBatch> inputs;
  {
    auto input_schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    std::shared_ptr<arrow::Array> array_0;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({1, 2, 3, 4}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
    double_builder.Reset();
    inputs = MakeRecordBatch(input_schema, 4, {array_0});
  }
  // run
  auto output = executor->Run(inputs);

  // build expect
  auto expect_output_schema =
      arrow::schema({arrow::field("test_field_a", arrow::utf8())});
  std::shared_ptr<arrow::Array> expect_array;
  arrow::StringBuilder str_builder;
  SERVING_CHECK_ARROW_STATUS(str_builder.AppendValues({"5", "7", "9", "11"}));
  SERVING_CHECK_ARROW_STATUS(str_builder.Finish(&expect_array));

  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.at(0).node_name, "node_d");
  EXPECT_EQ(output.at(0).table->num_columns(), 1);
  EXPECT_TRUE(output.at(0).table->schema()->Equals(expect_output_schema));

  std::cout << output.at(0).table->column(0)->ToString() << std::endl;
  std::cout << expect_array->ToString() << std::endl;

  EXPECT_TRUE(output.at(0).table->column(0)->Equals(expect_array));
}

TEST_F(ExecutorTest, ExceptionWorks) {
  std::vector<std::string> node_def_jsons = {
      R"JSON(
{
  "name": "node_a",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_b",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_c",
  "op": "TEST_OP_ONE_EDGE_ADD_MAX",
  "parents": [ "node_a" ],
}
)JSON",
      R"JSON(
{
  "name": "node_d",
  "op": "TEST_OP_TWO_EDGE_ADD_TO_STR",
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
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes), true, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  std::shared_ptr<arrow::RecordBatch> inputs;
  {
    auto input_schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    std::shared_ptr<arrow::Array> array_0;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({11, 22, 33, 44}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
    double_builder.Reset();
    inputs = MakeRecordBatch(input_schema, 4, {array_0});
  }
  // run
  EXPECT_THROW(executor->Run(inputs), ::secretflow::serving::Exception);

  // expect
  EXPECT_EQ(ThreadPool::GetInstance()->GetTaskSize(), 0);
}

TEST_F(ExecutorTest, ExceptionComplexMassiveWorks) {
  std::vector<std::string> node_def_jsons;
  std::string node_list_str;
  node_def_jsons.emplace_back(
      MakeNodeDefJson("node_a", "TEST_OP_ONE_EDGE_ADD_ONE"));
  node_list_str += R"("node_a",)";

  std::vector<std::string> last_level_parents;
  for (auto i = 0; i != MASSIVE_NODE_CNT - 1; ++i) {
    std::string node_name = "node_b_" + std::to_string(i);
    node_list_str += '"' + node_name + '"' + ',';

    node_def_jsons.emplace_back(
        MakeNodeDefJson(node_name, "TEST_OP_ONE_EDGE_ADD_ONE", {"node_a"}));
    last_level_parents.emplace_back(std::move(node_name));
  }

  // exception node
  std::string node_name = "node_b_" + std::to_string(MASSIVE_NODE_CNT - 1);
  node_list_str += '"' + node_name + '"' + ',';
  node_def_jsons.emplace_back(
      MakeNodeDefJson(node_name, "TEST_OP_ONE_EDGE_ADD_MAX", {"node_a"}));
  last_level_parents.emplace_back(std::move(node_name));

  unsigned node_c_count = MASSIVE_NODE_CNT / 2;
  for (unsigned i = 0; i != node_c_count; ++i) {
    std::string node_name = "node_c_" + std::to_string(i);
    node_list_str += '"' + node_name + '"' + ',';

    node_def_jsons.emplace_back(
        MakeNodeDefJson(node_name, "TEST_OP_REDUCE_2",
                        {"node_b_" + std::to_string(i * 2),
                         "node_b_" + std::to_string(i * 2 + 1)}));
    last_level_parents.emplace_back(std::move(node_name));
  }

  node_def_jsons.emplace_back(MakeNodeDefJson(
      "node_d", "TEST_OP_REDUCE_COMPLEX_MASSIVE_CNT", last_level_parents));
  node_list_str += R"("node_d")";

  std::string execution_def_json =
      R"({"nodes": [)" + node_list_str +
      R"(],"config": {"dispatch_type": "DP_ALL"} })";

  // build node
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(0, std::move(executino_def),
                                               std::move(nodes), true, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  std::shared_ptr<arrow::RecordBatch> inputs;
  {
    auto input_schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64())});
    std::shared_ptr<arrow::Array> array_0;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({11, 22, 33, 44}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
    double_builder.Reset();
    inputs = MakeRecordBatch(input_schema, 4, {array_0});
  }
  // run
  EXPECT_THROW(executor->Run(inputs), ::secretflow::serving::Exception);

  // wait for thread pool to pop remain tasks
  executor.reset();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // expect
  EXPECT_EQ(ThreadPool::GetInstance()->GetTaskSize(), 0);
}

TEST_F(ExecutorTest, PrevNodeDataInput) {
  std::vector<std::string> node_def_jsons = {
      R"JSON(
{
  "name": "node_a",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_b",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
}
)JSON",
      R"JSON(
{
  "name": "node_c",
  "op": "TEST_OP_ONE_EDGE_ADD_ONE",
  "parents": [ "node_a" ],
}
)JSON",
      R"JSON(
{
  "name": "node_d",
  "op": "TEST_OP_TWO_EDGE_ADD_TO_STR",
  "parents": [ "node_b",  "node_c" ],
}
)JSON"};

  std::string execution_def_json = R"JSON(
{
  "nodes": [
    "node_c", "node_d"
  ],
  "config": {
    "dispatch_type": "DP_ANYONE"
  }
}
)JSON";

  // build node
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
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
      n_iter->second->AddOutEdge(edge);
      pair.second->AddInEdge(edge);
    }
  }

  ExecutionDef executino_def;
  JsonToPb(execution_def_json, &executino_def);

  auto execution = std::make_shared<Execution>(
      1, std::move(executino_def),
      std::unordered_map<std::string, std::shared_ptr<Node>>{
          {"node_c", nodes["node_c"]}, {"node_d", nodes["node_d"]}},
      false, true);
  auto executor = std::make_shared<Executor>(execution);

  // mock input
  auto input_schema =
      arrow::schema({arrow::field("test_field_0", arrow::float64())});
  std::shared_ptr<arrow::Array> array_0;
  std::shared_ptr<arrow::Array> array_1;
  arrow::DoubleBuilder double_builder;
  SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({2, 3, 4, 5}));
  SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_0));
  double_builder.Reset();
  SERVING_CHECK_ARROW_STATUS(double_builder.AppendValues({12, 23, 34, 45}));
  SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array_1));
  double_builder.Reset();
  auto output_a = MakeRecordBatch(input_schema, 4, {array_0});
  auto output_b = MakeRecordBatch(input_schema, 4, {array_1});
  auto prev_node_io =
      std::unordered_map<std::string,
                         std::vector<std::shared_ptr<arrow::RecordBatch>>>();
  prev_node_io.emplace(
      "node_a", std::vector<std::shared_ptr<arrow::RecordBatch>>{output_a});
  prev_node_io.emplace(
      "node_b", std::vector<std::shared_ptr<arrow::RecordBatch>>{output_b});

  // run
  auto output = executor->Run(prev_node_io);

  // build expect
  auto expect_output_schema =
      arrow::schema({arrow::field("test_field_a", arrow::utf8())});
  std::shared_ptr<arrow::Array> expect_array;
  arrow::StringBuilder str_builder;
  SERVING_CHECK_ARROW_STATUS(
      str_builder.AppendValues({"15", "27", "39", "51"}));
  SERVING_CHECK_ARROW_STATUS(str_builder.Finish(&expect_array));

  EXPECT_EQ(output.size(), 1);
  EXPECT_EQ(output.at(0).node_name, "node_d");
  EXPECT_EQ(output.at(0).table->num_columns(), 1);
  EXPECT_TRUE(output.at(0).table->schema()->Equals(expect_output_schema));

  std::cout << output.at(0).table->column(0)->ToString() << std::endl;
  std::cout << expect_array->ToString() << std::endl;

  EXPECT_TRUE(output.at(0).table->column(0)->Equals(expect_array));
}

}  // namespace secretflow::serving
