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

TEST_F(ArrowHelperTest, FeaturesToRecordBatch) {
  ::google::protobuf::RepeatedPtrField<Feature> features;

  std::vector<int8_t> int8_list = {1, -2, -3};
  std::vector<int16_t> int16_list = {1, -2, -3};
  std::vector<int32_t> int32_list = {1, -2, -3};
  std::vector<uint8_t> uint8_list = {11, 22, 33};
  std::vector<uint16_t> uint16_list = {1, 22, 33};
  std::vector<uint32_t> uint32_list = {2294967296, 2294967297, 2294967298};
  std::vector<int64_t> int64_list = {4294967296, -4294967297, 4294967298};
  std::vector<uint64_t> uint64_list = {4294967296, 4294967297, 4294967298};
  std::vector<float> float_list = {1.1F, 2.2F, 3.3F};
  std::vector<double> double_list = {16777217.1, 16777218.2, 16777218.3};
  std::vector<std::string> str_list = {"test_0", "test_1", "test_2"};
  std::vector<bool> bool_list = {true, false, false};

  std::vector<std::shared_ptr<arrow::Field>> fields = {
      arrow::field("int8", arrow::int8()),
      arrow::field("uint8", arrow::uint8()),
      arrow::field("int16", arrow::int16()),
      arrow::field("uint16", arrow::uint16()),
      arrow::field("int32", arrow::int32()),
      arrow::field("uint32", arrow::uint32()),
      arrow::field("int64", arrow::int64()),
      arrow::field("uint64", arrow::uint64()),
      arrow::field("float", arrow::float32()),
      arrow::field("double", arrow::float64()),
      arrow::field("string", arrow::utf8()),
      arrow::field("binary", arrow::binary()),
      arrow::field("bool", arrow::boolean())};

  std::vector<std::shared_ptr<arrow::Array>> arrays;
  for (const auto& f : fields) {
    std::shared_ptr<arrow::Array> array;
    switch (f->type()->id()) {
      case arrow::Type::INT8: {
        auto* i32_f = features.Add();
        i32_f->mutable_field()->set_name("int8");
        i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
        i32_f->mutable_value()->mutable_i32s()->Assign(int8_list.begin(),
                                                       int8_list.end());
        arrow::Int8Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(int8_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT8: {
        auto* i32_f = features.Add();
        i32_f->mutable_field()->set_name("uint8");
        i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
        i32_f->mutable_value()->mutable_i32s()->Assign(uint8_list.begin(),
                                                       uint8_list.end());
        arrow::UInt8Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(uint8_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::INT16: {
        auto* i32_f = features.Add();
        i32_f->mutable_field()->set_name("int16");
        i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
        i32_f->mutable_value()->mutable_i32s()->Assign(int16_list.begin(),
                                                       int16_list.end());
        arrow::Int16Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(int16_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT16: {
        auto* i32_f = features.Add();
        i32_f->mutable_field()->set_name("uint16");
        i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
        i32_f->mutable_value()->mutable_i32s()->Assign(uint16_list.begin(),
                                                       uint16_list.end());
        arrow::UInt16Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(uint16_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::INT32: {
        auto* i32_f = features.Add();
        i32_f->mutable_field()->set_name("int32");
        i32_f->mutable_field()->set_type(FieldType::FIELD_INT32);
        i32_f->mutable_value()->mutable_i32s()->Assign(int32_list.begin(),
                                                       int32_list.end());
        arrow::Int32Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(int32_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT32: {
        auto* i64_f = features.Add();
        i64_f->mutable_field()->set_name("uint32");
        i64_f->mutable_field()->set_type(FieldType::FIELD_INT64);
        i64_f->mutable_value()->mutable_i64s()->Assign(uint32_list.begin(),
                                                       uint32_list.end());
        arrow::UInt32Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(uint32_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::INT64: {
        auto* i64_f = features.Add();
        i64_f->mutable_field()->set_name("int64");
        i64_f->mutable_field()->set_type(FieldType::FIELD_INT64);
        i64_f->mutable_value()->mutable_i64s()->Assign(int64_list.begin(),
                                                       int64_list.end());
        arrow::Int64Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(int64_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT64: {
        auto* i64_f = features.Add();
        i64_f->mutable_field()->set_name("uint64");
        i64_f->mutable_field()->set_type(FieldType::FIELD_INT64);
        i64_f->mutable_value()->mutable_i64s()->Assign(uint64_list.begin(),
                                                       uint64_list.end());
        arrow::UInt64Builder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(uint64_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::FLOAT: {
        auto* f_f = features.Add();
        f_f->mutable_field()->set_name("float");
        f_f->mutable_field()->set_type(FieldType::FIELD_FLOAT);
        f_f->mutable_value()->mutable_fs()->Assign(float_list.begin(),
                                                   float_list.end());
        arrow::FloatBuilder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(float_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::DOUBLE: {
        auto* d_f = features.Add();
        d_f->mutable_field()->set_name("double");
        d_f->mutable_field()->set_type(FieldType::FIELD_DOUBLE);
        d_f->mutable_value()->mutable_ds()->Assign(double_list.begin(),
                                                   double_list.end());
        arrow::DoubleBuilder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(double_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::STRING: {
        auto* s_f = features.Add();
        s_f->mutable_field()->set_name("string");
        s_f->mutable_field()->set_type(FieldType::FIELD_STRING);
        s_f->mutable_value()->mutable_ss()->Assign(str_list.begin(),
                                                   str_list.end());
        arrow::StringBuilder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(str_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::BINARY: {
        auto* s_f = features.Add();
        s_f->mutable_field()->set_name("binary");
        s_f->mutable_field()->set_type(FieldType::FIELD_STRING);
        s_f->mutable_value()->mutable_ss()->Assign(str_list.begin(),
                                                   str_list.end());
        arrow::BinaryBuilder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(str_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      case arrow::Type::BOOL: {
        auto* bool_f = features.Add();
        bool_f->mutable_field()->set_name("bool");
        bool_f->mutable_field()->set_type(FieldType::FIELD_BOOL);
        bool_f->mutable_value()->mutable_bs()->Assign(bool_list.begin(),
                                                      bool_list.end());
        arrow::BooleanBuilder builder;
        SERVING_CHECK_ARROW_STATUS(builder.AppendValues(bool_list));
        SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "logistic error");
    }
    arrays.emplace_back(std::move(array));
  }

  // expect
  auto expect_schema = arrow::schema(fields);
  auto expect_record = MakeRecordBatch(expect_schema, 3, arrays);

  std::cout << "expect_record: " << expect_record->ToString() << std::endl;

  auto record_batch = FeaturesToRecordBatch(features, expect_schema);

  std::cout << "record_batch: " << record_batch->ToString() << std::endl;

  EXPECT_TRUE(record_batch->schema()->Equals(expect_schema));
  EXPECT_TRUE(record_batch->Equals(*expect_record));
}

// TODO: exception case

}  // namespace secretflow::serving::op
