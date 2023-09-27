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

#include <arrow/io/api.h>
#include <arrow/ipc/api.h>

namespace secretflow::serving {

std::shared_ptr<arrow::DataType> FieldTypeToDataType(FieldType field_type) {
  const static std::map<FieldType, std::shared_ptr<arrow::DataType>>
      kDataTypeMap = {
          {FieldType::FIELD_BOOL, arrow::boolean()},
          {FieldType::FIELD_INT32, arrow::int32()},
          {FieldType::FIELD_INT64, arrow::int64()},
          {FieldType::FIELD_FLOAT, arrow::float32()},
          {FieldType::FIELD_DOUBLE, arrow::float64()},
          {FieldType::FIELD_STRING, arrow::utf8()},
      };

  auto it = kDataTypeMap.find(field_type);
  SERVING_ENFORCE(it != kDataTypeMap.end(), errors::ErrorCode::LOGIC_ERROR,
                  "unknow field type: {}", FieldType_Name(field_type));
  return it->second;
}

FieldType DataTypeToFieldType(
    const std::shared_ptr<arrow::DataType>& data_type) {
  const static std::map<arrow::Type::type, FieldType> kFieldTypeMap = {
      {arrow::Type::type::BOOL, FieldType::FIELD_BOOL},
      {arrow::Type::type::INT32, FieldType::FIELD_INT32},
      {arrow::Type::type::INT64, FieldType::FIELD_INT64},
      {arrow::Type::type::FLOAT, FieldType::FIELD_FLOAT},
      {arrow::Type::type::DOUBLE, FieldType::FIELD_DOUBLE},
      {arrow::Type::type::STRING, FieldType::FIELD_STRING},
  };

  auto it = kFieldTypeMap.find(data_type->id());
  SERVING_ENFORCE(it != kFieldTypeMap.end(), errors::ErrorCode::LOGIC_ERROR,
                  "unsupport arrow data type: {}",
                  arrow::internal::ToString(data_type->id()));
  return it->second;
}

std::shared_ptr<arrow::RecordBatch> FeaturesToTable(
    const ::google::protobuf::RepeatedPtrField<Feature>& features) {
  arrow::SchemaBuilder schema_builder;
  std::vector<std::shared_ptr<arrow::Array>> arrays;
  int num_rows = -1;
  for (const auto& f : features) {
    std::shared_ptr<arrow::Array> array;
    int cur_num_rows = -1;
    switch (f.field().type()) {
      case FieldType::FIELD_BOOL: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::boolean())));
        arrow::BooleanBuilder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
            f.value().bs().begin(), f.value().bs().end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().bs_size();
        break;
      }
      case FieldType::FIELD_INT32: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::int32())));
        arrow::Int32Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
            f.value().i32s().begin(), f.value().i32s().end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().i32s_size();
        break;
      }
      case FieldType::FIELD_INT64: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::int64())));
        arrow::Int64Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
            f.value().i64s().begin(), f.value().i64s().end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().i64s_size();
        break;
      }
      case FieldType::FIELD_FLOAT: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::float32())));
        arrow::FloatBuilder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
            f.value().fs().begin(), f.value().fs().end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().fs_size();
        break;
      }
      case FieldType::FIELD_DOUBLE: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::float64())));
        arrow::DoubleBuilder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.AppendValues(
            f.value().ds().begin(), f.value().ds().end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().ds_size();
        break;
      }
      case FieldType::FIELD_STRING: {
        SERVING_CHECK_ARROW_STATUS(schema_builder.AddField(
            arrow::field(f.field().name(), arrow::utf8())));
        arrow::StringBuilder array_builder;
        auto ss_list = f.value().ss();
        for (const auto& s : ss_list) {
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(s));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        cur_num_rows = f.value().ss_size();
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "unkown field type",
                      FieldType_Name(f.field().type()));
    }
    if (num_rows >= 0) {
      SERVING_ENFORCE_EQ(num_rows, cur_num_rows,
                         "features must have same length value.");
    }
    num_rows = cur_num_rows;
    arrays.emplace_back(array);
  }
  std::shared_ptr<arrow::Schema> schema;
  SERVING_GET_ARROW_RESULT(schema_builder.Finish(), schema);
  return MakeRecordBatch(schema, num_rows, std::move(arrays));
}

std::string SerializeRecordBatch(
    std::shared_ptr<arrow::RecordBatch>& recordBatch) {
  std::shared_ptr<arrow::io::BufferOutputStream> out_stream;
  SERVING_GET_ARROW_RESULT(arrow::io::BufferOutputStream::Create(), out_stream);

  std::shared_ptr<arrow::ipc::RecordBatchWriter> writer;
  SERVING_GET_ARROW_RESULT(
      arrow::ipc::MakeStreamWriter(out_stream, recordBatch->schema()), writer);

  SERVING_CHECK_ARROW_STATUS(writer->WriteRecordBatch(*recordBatch));
  SERVING_CHECK_ARROW_STATUS(writer->Close());

  std::shared_ptr<arrow::Buffer> buffer;
  SERVING_GET_ARROW_RESULT(out_stream->Finish(), buffer);
  return buffer->ToString();
}

std::shared_ptr<arrow::RecordBatch> DeserializeRecordBatch(
    const std::string& buf) {
  auto buf_reader = std::make_shared<arrow::io::BufferReader>(buf);

  std::shared_ptr<arrow::ipc::RecordBatchReader> reader;
  SERVING_GET_ARROW_RESULT(
      arrow::ipc::RecordBatchStreamReader::Open(buf_reader), reader);

  std::vector<std::shared_ptr<arrow::RecordBatch>> record_batches;
  SERVING_GET_ARROW_RESULT(reader->ToRecordBatches(), record_batches);
  SERVING_ENFORCE(record_batches.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  auto record_batch = record_batches.front();
  SERVING_CHECK_ARROW_STATUS(record_batch->Validate());

  return record_batch;
}

}  // namespace secretflow::serving
