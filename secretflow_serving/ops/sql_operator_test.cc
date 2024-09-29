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

#include "secretflow_serving/ops/sql_operator.h"

#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

class SqlOperatorTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// 用例一
TEST_F(SqlOperatorTest, Works1) {
  AttrValue tbl_name;
  tbl_name.set_s("test_sql");

  AttrValue input_feature_names;
  {
    std::vector<std::string> data = {"x1", "x2"};
    input_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue input_feature_types;
  {
    std::vector<std::string> data = {"double", "string"};
    input_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue output_feature_names;
  {
    std::vector<std::string> data = {"a1", "a2", "x1", "x2"};
    output_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }
  AttrValue output_feature_types;
  {
    std::vector<std::string> data = {"double", "string", "double", "string"};
    output_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }
  AttrValue sql;
  sql.set_s("SELECT x1 as a1, x2 as a2 FROM test_sql");

  // 构建节点数据
  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("SQL_OPERATOR");
  node_def.mutable_attr_values()->insert({"tbl_name", tbl_name});
  node_def.mutable_attr_values()->insert(
      {"input_feature_names", input_feature_names});
  node_def.mutable_attr_values()->insert(
      {"input_feature_types", input_feature_types});
  node_def.mutable_attr_values()->insert(
      {"output_feature_names", output_feature_names});
  node_def.mutable_attr_values()->insert(
      {"output_feature_types", output_feature_types});
  node_def.mutable_attr_values()->insert({"sql", sql});
  auto mock_node = std::make_shared<Node>(std::move(node_def));

  // 根据节点数据初始化算子
  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  ComputeContext compute_ctx;
  compute_ctx.inputs = OpComputeInputs();
  // 构造测试数据
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1, array2;

    arrow::DoubleBuilder double_builder1;
    SERVING_CHECK_ARROW_STATUS(double_builder1.AppendValues({0.6, 0.5}));
    SERVING_CHECK_ARROW_STATUS(double_builder1.Finish(&array1));
    arrays.emplace_back(array1);
    arrow::StringBuilder string_builder;
    SERVING_CHECK_ARROW_STATUS(string_builder.AppendValues({"test6", "test5"}));
    SERVING_CHECK_ARROW_STATUS(string_builder.Finish(&array2));
    arrays.emplace_back(array2);
    const auto& input_schema_list = kernel->GetAllInputSchema();
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }
  // 算子执行
  kernel->Compute(&compute_ctx);

  // 检查执行结果
  ASSERT_TRUE(compute_ctx.output);
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  std::shared_ptr<arrow::RecordBatch> test_batch;
  {
    std::vector<std::shared_ptr<arrow::Field>> f_list;
    f_list.emplace_back(arrow::field("a1", arrow::float64()));
    f_list.emplace_back(arrow::field("a2", arrow::utf8()));
    f_list.emplace_back(arrow::field("x1", arrow::float64()));
    f_list.emplace_back(arrow::field("x2", arrow::utf8()));
    auto test_schema = arrow::schema(std::move(f_list));

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1, array2, array3, array4;
    arrow::DoubleBuilder builder1;
    SERVING_CHECK_ARROW_STATUS(builder1.AppendValues({0.6, 0.5}));
    SERVING_CHECK_ARROW_STATUS(builder1.Finish(&array1));
    arrays.emplace_back(array1);
    arrow::StringBuilder builder2;
    SERVING_CHECK_ARROW_STATUS(builder2.AppendValues({"test6", "test5"}));
    SERVING_CHECK_ARROW_STATUS(builder2.Finish(&array2));
    arrays.emplace_back(array2);
    arrow::DoubleBuilder builder3;
    SERVING_CHECK_ARROW_STATUS(builder3.AppendValues({0.6, 0.5}));
    SERVING_CHECK_ARROW_STATUS(builder3.Finish(&array3));
    arrays.emplace_back(array3);
    arrow::StringBuilder builder4;
    SERVING_CHECK_ARROW_STATUS(builder4.AppendValues({"test6", "test5"}));
    SERVING_CHECK_ARROW_STATUS(builder4.Finish(&array4));
    arrays.emplace_back(array4);
    test_batch = MakeRecordBatch(test_schema, 2, std::move(arrays));
  }
  std::cout << compute_ctx.output->ToString() << std::endl;
  std::cout << test_batch->ToString() << std::endl;
  ASSERT_TRUE(compute_ctx.output->Equals(*test_batch));
}

// 用例二 sql为空则数据原封不动输出
TEST_F(SqlOperatorTest, Works2) {
  AttrValue tbl_name;
  tbl_name.set_s("test_sql");

  AttrValue input_feature_names;
  {
    std::vector<std::string> data = {"x1", "x2"};
    input_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue input_feature_types;
  {
    std::vector<std::string> data = {"double", "string"};
    input_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue output_feature_names;
  {
    std::vector<std::string> data = {"x1", "x2"};
    output_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }
  AttrValue output_feature_types;
  {
    std::vector<std::string> data = {"double", "string"};
    output_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }
  AttrValue sql;
  sql.set_s("");

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("SQL_OPERATOR");
  node_def.mutable_attr_values()->insert({"tbl_name", tbl_name});
  node_def.mutable_attr_values()->insert(
      {"input_feature_names", input_feature_names});
  node_def.mutable_attr_values()->insert(
      {"input_feature_types", input_feature_types});
  node_def.mutable_attr_values()->insert(
      {"output_feature_names", output_feature_names});
  node_def.mutable_attr_values()->insert(
      {"output_feature_types", output_feature_types});
  node_def.mutable_attr_values()->insert({"sql", sql});
  auto mock_node = std::make_shared<Node>(std::move(node_def));

  // 根据节点数据初始化算子
  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = OpComputeInputs();
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1, array2;

    arrow::DoubleBuilder double_builder1;
    SERVING_CHECK_ARROW_STATUS(double_builder1.AppendValues({0.6, 0.5}));
    SERVING_CHECK_ARROW_STATUS(double_builder1.Finish(&array1));
    arrays.emplace_back(array1);
    arrow::StringBuilder string_builder;
    SERVING_CHECK_ARROW_STATUS(string_builder.AppendValues({"test6", "test5"}));
    SERVING_CHECK_ARROW_STATUS(string_builder.Finish(&array2));
    arrays.emplace_back(array2);
    const auto& input_schema_list = kernel->GetAllInputSchema();
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));

    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }
  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(compute_ctx.output->schema()->Equals(output_schema));
  std::shared_ptr<arrow::RecordBatch> test_batch;
  {
    std::vector<std::shared_ptr<arrow::Field>> f_list;
    f_list.emplace_back(arrow::field("x1", arrow::float64()));
    f_list.emplace_back(arrow::field("x2", arrow::utf8()));
    auto test_schema = arrow::schema(std::move(f_list));

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1, array2, array3, array4;
    arrow::DoubleBuilder builder1;
    SERVING_CHECK_ARROW_STATUS(builder1.AppendValues({0.6, 0.5}));
    SERVING_CHECK_ARROW_STATUS(builder1.Finish(&array1));
    arrays.emplace_back(array1);
    arrow::StringBuilder builder2;
    SERVING_CHECK_ARROW_STATUS(builder2.AppendValues({"test6", "test5"}));
    SERVING_CHECK_ARROW_STATUS(builder2.Finish(&array2));
    arrays.emplace_back(array2);
    test_batch = MakeRecordBatch(test_schema, 2, std::move(arrays));
  }
  std::cout << compute_ctx.output->ToString() << std::endl;
  std::cout << test_batch->ToString() << std::endl;
  ASSERT_TRUE(compute_ctx.output->Equals(*test_batch));
}

}  // namespace secretflow::serving::op
