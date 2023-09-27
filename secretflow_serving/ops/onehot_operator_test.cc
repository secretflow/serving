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

#include "secretflow_serving/ops/onehot_operator.h"

#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op {

class OnehotOperatorTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OnehotOperatorTest, Works1) {
  // mock attr
  AttrValue rule_json;
  rule_json.set_s(
      "{\"feature_mappings\":[{\"categories\":[{\"mapping_name\":\"province_"
      "insured_m_0\",\"value\":\"shanghai\"},{\"mapping_name\":\"province_"
      "insured_m_1\",\"value\":\"zhejiang\"},{\"mapping_name\":\"province_"
      "insured_m_2\",\"value\":\"jiangsu\"},{\"mapping_name\":\"province_"
      "insured_m_3\",\"value\":\"hainan\"},{\"mapping_name\":\"province_"
      "insured_m_4\",\"value\":\"xiamen\"}],\"raw_name\":\"province_insured_"
      "m\"},{\"categories\":[{\"mapping_name\":\"renew_m_0\",\"value\":\"A\"},{"
      "\"mapping_name\":\"renew_m_1\",\"value\":\"B\"},{\"mapping_name\":"
      "\"renew_m_2\",\"value\":\"C\"}],\"raw_name\":\"renew_m\"}]}");

  AttrValue input_feature_names;
  {
    std::vector<std::string> data = {"province_insured_m", "test_col",
                                     "renew_m"};
    input_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue input_feature_types;
  {
    std::vector<std::string> data = {"string", "double", "string"};
    input_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue output_feature_names;
  {
    std::vector<std::string> data = {"province_insured_m_0",
                                     "province_insured_m_1",
                                     "province_insured_m_2",
                                     "province_insured_m_3",
                                     "province_insured_m_4",
                                     "test_col",
                                     "renew_m_0",
                                     "renew_m_1",
                                     "renew_m_2"};
    output_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  AttrValue output_feature_types;
  {
    std::vector<std::string> data = {"double", "double", "double",
                                     "double", "double", "double",
                                     "double", "double", "double"};
    output_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("ONEHOT_OPERATOR");
  node_def.mutable_attr_values()->insert({"rule_json", rule_json});
  node_def.mutable_attr_values()->insert(
      {"input_feature_names", input_feature_names});
  node_def.mutable_attr_values()->insert(
      {"input_feature_types", input_feature_types});
  node_def.mutable_attr_values()->insert(
      {"output_feature_names", output_feature_names});
  node_def.mutable_attr_values()->insert(
      {"output_feature_types", output_feature_types});
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  OpKernelOptions opts{mock_node};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = std::make_shared<OpComputeInputs>();
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1;
    arrow::StringBuilder string_builder;
    SERVING_CHECK_ARROW_STATUS(
        string_builder.AppendValues({"zhejiang", "jiangsu"}));
    SERVING_CHECK_ARROW_STATUS(string_builder.Finish(&array1));
    arrays.emplace_back(array1);
    std::shared_ptr<arrow::Array> array2;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(
        double_builder.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array2));
    arrays.emplace_back(array2);
    std::shared_ptr<arrow::Array> array3;
    arrow::StringBuilder string_builder2;
    SERVING_CHECK_ARROW_STATUS(string_builder2.AppendValues({"A", "B"}));
    SERVING_CHECK_ARROW_STATUS(string_builder2.Finish(&array3));
    arrays.emplace_back(array3);

    const auto& input_schema_list = kernel->GetAllInputSchema();
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));
    compute_ctx.inputs->emplace_back(
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
    f_list.emplace_back(arrow::field("province_insured_m_0", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_1", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_2", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_3", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_4", arrow::float64()));
    f_list.emplace_back(arrow::field("test_col", arrow::float64()));
    f_list.emplace_back(arrow::field("renew_m_0", arrow::float64()));
    f_list.emplace_back(arrow::field("renew_m_1", arrow::float64()));
    f_list.emplace_back(arrow::field("renew_m_2", arrow::float64()));
    auto test_schema = arrow::schema(std::move(f_list));

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1;
    arrow::DoubleBuilder builder1;
    SERVING_CHECK_ARROW_STATUS(builder1.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder1.Finish(&array1));
    arrays.emplace_back(array1);
    std::shared_ptr<arrow::Array> array2;
    arrow::DoubleBuilder builder2;
    SERVING_CHECK_ARROW_STATUS(builder2.AppendValues({1, 0}));
    SERVING_CHECK_ARROW_STATUS(builder2.Finish(&array2));
    arrays.emplace_back(array2);
    std::shared_ptr<arrow::Array> array3;
    arrow::DoubleBuilder builder3;
    SERVING_CHECK_ARROW_STATUS(builder3.AppendValues({0, 1}));
    SERVING_CHECK_ARROW_STATUS(builder3.Finish(&array3));
    arrays.emplace_back(array3);
    std::shared_ptr<arrow::Array> array4;
    arrow::DoubleBuilder builder4;
    SERVING_CHECK_ARROW_STATUS(builder4.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder4.Finish(&array4));
    arrays.emplace_back(array4);
    std::shared_ptr<arrow::Array> array5;
    arrow::DoubleBuilder builder5;
    SERVING_CHECK_ARROW_STATUS(builder5.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder5.Finish(&array5));
    arrays.emplace_back(array5);
    std::shared_ptr<arrow::Array> array6;
    arrow::DoubleBuilder builder6;
    SERVING_CHECK_ARROW_STATUS(builder6.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(builder6.Finish(&array6));
    arrays.emplace_back(array6);
    std::shared_ptr<arrow::Array> array7;
    arrow::DoubleBuilder builder7;
    SERVING_CHECK_ARROW_STATUS(builder7.AppendValues({1, 0}));
    SERVING_CHECK_ARROW_STATUS(builder7.Finish(&array7));
    arrays.emplace_back(array7);
    std::shared_ptr<arrow::Array> array8;
    arrow::DoubleBuilder builder8;
    SERVING_CHECK_ARROW_STATUS(builder8.AppendValues({0, 1}));
    SERVING_CHECK_ARROW_STATUS(builder8.Finish(&array8));
    arrays.emplace_back(array8);
    std::shared_ptr<arrow::Array> array9;
    arrow::DoubleBuilder builder9;
    SERVING_CHECK_ARROW_STATUS(builder9.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder9.Finish(&array9));
    arrays.emplace_back(array9);
    test_batch = MakeRecordBatch(test_schema, 2, std::move(arrays));
  }
  std::cout << compute_ctx.output->ToString() << std::endl;
  std::cout << test_batch->ToString() << std::endl;
  ASSERT_TRUE(compute_ctx.output->Equals(*test_batch));
}

// TODO: exception case
TEST_F(OnehotOperatorTest, Works2) {
  // mock attr
  AttrValue rule_json;
  rule_json.set_s("");

  AttrValue input_feature_names;
  {
    std::vector<std::string> data = {"gender_m", "test_col"};
    input_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue input_feature_types;
  {
    std::vector<std::string> data = {"string", "double"};
    input_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue output_feature_names;
  {
    std::vector<std::string> data = {"gender_m", "test_col"};
    output_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  AttrValue output_feature_types;
  {
    std::vector<std::string> data = {"string", "double"};
    output_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("ONEHOT_OPERATOR");
  node_def.mutable_attr_values()->insert({"rule_json", rule_json});
  node_def.mutable_attr_values()->insert(
      {"input_feature_names", input_feature_names});
  node_def.mutable_attr_values()->insert(
      {"input_feature_types", input_feature_types});
  node_def.mutable_attr_values()->insert(
      {"output_feature_names", output_feature_names});
  node_def.mutable_attr_values()->insert(
      {"output_feature_types", output_feature_types});
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  OpKernelOptions opts{mock_node};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = std::make_shared<OpComputeInputs>();
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1;
    arrow::StringBuilder string_builder;
    SERVING_CHECK_ARROW_STATUS(string_builder.AppendValues({"1.0", "0.0"}));
    SERVING_CHECK_ARROW_STATUS(string_builder.Finish(&array1));
    arrays.emplace_back(array1);
    std::shared_ptr<arrow::Array> array2;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(
        double_builder.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array2));
    arrays.emplace_back(array2);

    const auto& input_schema_list = kernel->GetAllInputSchema();
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));
    compute_ctx.inputs->emplace_back(
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
    f_list.emplace_back(arrow::field("gender_m", arrow::utf8()));
    f_list.emplace_back(arrow::field("test_col", arrow::float64()));
    auto test_schema = arrow::schema(std::move(f_list));

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array2;
    arrow::StringBuilder builder2;
    SERVING_CHECK_ARROW_STATUS(builder2.AppendValues({"1.0", "0.0"}));
    SERVING_CHECK_ARROW_STATUS(builder2.Finish(&array2));
    arrays.emplace_back(array2);
    std::shared_ptr<arrow::Array> array3;
    arrow::DoubleBuilder builder3;
    SERVING_CHECK_ARROW_STATUS(builder3.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(builder3.Finish(&array3));
    arrays.emplace_back(array3);
    test_batch = MakeRecordBatch(test_schema, 2, std::move(arrays));
  }
  std::cout << compute_ctx.output->ToString() << std::endl;
  std::cout << test_batch->ToString() << std::endl;
  ASSERT_TRUE(compute_ctx.output->Equals(*test_batch));
}

TEST_F(OnehotOperatorTest, Works3) {
  // mock attr
  AttrValue rule_json;
  rule_json.set_s(
      "{\"feature_mappings\":[{\"raw_name\":\"province_insured_m\","
      "\"categories\":[{\"mapping_name\":\"province_insured_m_0\",\"value\":"
      "\"shanghai\"},{\"mapping_name\":\"province_insured_m_1\",\"value\":"
      "\"zhejiang\"},{\"mapping_name\":\"province_insured_m_2\",\"value\":"
      "\"jiangsu\"},{\"mapping_name\":\"province_insured_m_3\",\"value\":"
      "\"hainan\"},{\"mapping_name\":\"province_insured_m_4\",\"value\":"
      "\"xiamen\"}]}]}");

  AttrValue input_feature_names;
  {
    std::vector<std::string> data = {"province_insured_m", "test_col"};
    input_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue input_feature_types;
  {
    std::vector<std::string> data = {"string", "double"};
    input_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                             data.end());
  }

  AttrValue output_feature_names;
  {
    std::vector<std::string> data = {
        "province_insured_m_0", "province_insured_m_1", "province_insured_m_2",
        "province_insured_m_3", "province_insured_m_4", "test_col"};
    output_feature_names.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  AttrValue output_feature_types;
  {
    std::vector<std::string> data = {"double", "double", "double",
                                     "double", "double", "double"};
    output_feature_types.mutable_ss()->mutable_data()->Assign(data.begin(),
                                                              data.end());
  }

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("ONEHOT_OPERATOR");
  node_def.mutable_attr_values()->insert({"rule_json", rule_json});
  node_def.mutable_attr_values()->insert(
      {"input_feature_names", input_feature_names});
  node_def.mutable_attr_values()->insert(
      {"input_feature_types", input_feature_types});
  node_def.mutable_attr_values()->insert(
      {"output_feature_names", output_feature_names});
  node_def.mutable_attr_values()->insert(
      {"output_feature_types", output_feature_types});

  auto mock_node = std::make_shared<Node>(std::move(node_def));
  OpKernelOptions opts{mock_node};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs = std::make_shared<OpComputeInputs>();
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1;
    arrow::StringBuilder string_builder;
    SERVING_CHECK_ARROW_STATUS(
        string_builder.AppendValues({"zhejiang", "jiangsu"}));
    SERVING_CHECK_ARROW_STATUS(string_builder.Finish(&array1));
    arrays.emplace_back(array1);
    std::shared_ptr<arrow::Array> array2;
    arrow::DoubleBuilder double_builder;
    SERVING_CHECK_ARROW_STATUS(
        double_builder.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(double_builder.Finish(&array2));
    arrays.emplace_back(array2);

    const auto& input_schema_list = kernel->GetAllInputSchema();
    auto features =
        MakeRecordBatch(input_schema_list.front(), 2, std::move(arrays));
    compute_ctx.inputs->emplace_back(
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
    f_list.emplace_back(arrow::field("province_insured_m_0", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_1", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_2", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_3", arrow::float64()));
    f_list.emplace_back(arrow::field("province_insured_m_4", arrow::float64()));
    f_list.emplace_back(arrow::field("test_col", arrow::float64()));
    auto test_schema = arrow::schema(std::move(f_list));

    std::vector<std::shared_ptr<arrow::Array>> arrays;
    std::shared_ptr<arrow::Array> array1;
    arrow::DoubleBuilder builder1;
    SERVING_CHECK_ARROW_STATUS(builder1.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder1.Finish(&array1));
    arrays.emplace_back(array1);
    std::shared_ptr<arrow::Array> array2;
    arrow::DoubleBuilder builder2;
    SERVING_CHECK_ARROW_STATUS(builder2.AppendValues({1, 0}));
    SERVING_CHECK_ARROW_STATUS(builder2.Finish(&array2));
    arrays.emplace_back(array2);
    std::shared_ptr<arrow::Array> array3;
    arrow::DoubleBuilder builder3;
    SERVING_CHECK_ARROW_STATUS(builder3.AppendValues({0, 1}));
    SERVING_CHECK_ARROW_STATUS(builder3.Finish(&array3));
    arrays.emplace_back(array3);
    std::shared_ptr<arrow::Array> array4;
    arrow::DoubleBuilder builder4;
    SERVING_CHECK_ARROW_STATUS(builder4.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder4.Finish(&array4));
    arrays.emplace_back(array4);
    std::shared_ptr<arrow::Array> array5;
    arrow::DoubleBuilder builder5;
    SERVING_CHECK_ARROW_STATUS(builder5.AppendValues({0, 0}));
    SERVING_CHECK_ARROW_STATUS(builder5.Finish(&array5));
    arrays.emplace_back(array5);
    std::shared_ptr<arrow::Array> array6;
    arrow::DoubleBuilder builder6;
    SERVING_CHECK_ARROW_STATUS(builder6.AppendValues({2, 3.562436245624624}));
    SERVING_CHECK_ARROW_STATUS(builder6.Finish(&array6));
    arrays.emplace_back(array6);
    test_batch = MakeRecordBatch(test_schema, 2, std::move(arrays));
  }
  std::cout << compute_ctx.output->ToString() << std::endl;
  std::cout << test_batch->ToString() << std::endl;
  ASSERT_TRUE(compute_ctx.output->Equals(*test_batch));
}

}  // namespace secretflow::serving::op
