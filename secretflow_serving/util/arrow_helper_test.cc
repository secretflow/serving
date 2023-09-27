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

#include "secretflow_serving/util/arrow_helper.h"

#include "gtest/gtest.h"

namespace secretflow::serving::op {

class ArrowHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ArrowHelperTest, FeaturesToTable) {
  ::google::protobuf::RepeatedPtrField<Feature> features;
  // bool
  std::vector<bool> bool_list = {true, false, false};
  auto bool_f = features.Add();
  bool_f->mutable_field()->set_name("bool");
  bool_f->mutable_field()->set_type(FieldType::FIELD_BOOL);
  bool_f->mutable_value()->mutable_bs()->Assign(bool_list.begin(),
                                                bool_list.end());
  std::shared_ptr<arrow::Array> bool_array;
  arrow::BooleanBuilder bool_builder;
  SERVING_CHECK_ARROW_STATUS(bool_builder.AppendValues(bool_list));
  SERVING_CHECK_ARROW_STATUS(bool_builder.Finish(&bool_array));
  // int32
  std::vector<int32_t> int32_list = {1, 2, 3};
  auto i32_f = features.Add();
  i32_f->mutable_field()->set_name("int32");
  i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
  i32_f->mutable_value()->mutable_i32s()->Assign(int32_list.begin(),
                                                 int32_list.end());
  std::shared_ptr<arrow::Array> i32_array;
  arrow::Int32Builder i32_builder;
  SERVING_CHECK_ARROW_STATUS(i32_builder.AppendValues(int32_list));
  SERVING_CHECK_ARROW_STATUS(i32_builder.Finish(&i32_array));
  // int64
  std::vector<int64_t> int64_list = {4294967296, 4294967297, 4294967298};
  auto i64_f = features.Add();
  i64_f->mutable_field()->set_name("int64");
  i64_f->mutable_field()->set_type(FieldType::FIELD_INT64);
  i64_f->mutable_value()->mutable_i64s()->Assign(int64_list.begin(),
                                                 int64_list.end());
  std::shared_ptr<arrow::Array> i64_array;
  arrow::Int64Builder i64_builder;
  SERVING_CHECK_ARROW_STATUS(i64_builder.AppendValues(int64_list));
  SERVING_CHECK_ARROW_STATUS(i64_builder.Finish(&i64_array));
  // float
  std::vector<float> float_list = {1.1f, 2.2f, 3.3f};
  auto f_f = features.Add();
  f_f->mutable_field()->set_name("float");
  f_f->mutable_field()->set_type(FieldType::FIELD_FLOAT);
  f_f->mutable_value()->mutable_fs()->Assign(float_list.begin(),
                                             float_list.end());
  std::shared_ptr<arrow::Array> f_array;
  arrow::FloatBuilder f_builder;
  SERVING_CHECK_ARROW_STATUS(f_builder.AppendValues(float_list));
  SERVING_CHECK_ARROW_STATUS(f_builder.Finish(&f_array));
  // double
  std::vector<double> double_list = {16777217.1d, 16777218.2d, 16777218.3d};
  auto d_f = features.Add();
  d_f->mutable_field()->set_name("double");
  d_f->mutable_field()->set_type(FieldType::FIELD_DOUBLE);
  d_f->mutable_value()->mutable_ds()->Assign(double_list.begin(),
                                             double_list.end());
  std::shared_ptr<arrow::Array> d_array;
  arrow::DoubleBuilder d_builder;
  SERVING_CHECK_ARROW_STATUS(d_builder.AppendValues(double_list));
  SERVING_CHECK_ARROW_STATUS(d_builder.Finish(&d_array));
  // string
  std::vector<std::string> str_list = {"test_0", "test_1", "test_2"};
  auto s_f = features.Add();
  s_f->mutable_field()->set_name("string");
  s_f->mutable_field()->set_type(FieldType::FIELD_STRING);
  s_f->mutable_value()->mutable_ss()->Assign(str_list.begin(), str_list.end());
  std::shared_ptr<arrow::Array> str_array;
  arrow::StringBuilder str_builder;
  SERVING_CHECK_ARROW_STATUS(str_builder.AppendValues(str_list));
  SERVING_CHECK_ARROW_STATUS(str_builder.Finish(&str_array));

  auto record_batch = FeaturesToTable(features);

  // expect
  auto expect_schema = arrow::schema({arrow::field("bool", arrow::boolean()),
                                      arrow::field("int32", arrow::int32()),
                                      arrow::field("int64", arrow::int64()),
                                      arrow::field("float", arrow::float32()),
                                      arrow::field("double", arrow::float64()),
                                      arrow::field("string", arrow::utf8())});
  auto expect_record = MakeRecordBatch(
      expect_schema, 3,
      {bool_array, i32_array, i64_array, f_array, d_array, str_array});

  std::cout << record_batch->ToString() << std::endl;

  EXPECT_TRUE(record_batch->Equals(*expect_record));
}

// TODO: exception case

}  // namespace secretflow::serving::op
