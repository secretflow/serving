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

#include <algorithm>
#include <limits>
#include <random>

#include "arrow/compute/api.h"
#include "arrow/io/api.h"
#include "arrow/ipc/api.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace {

struct FeatureToArrayVisitor {
  void operator()(const FeatureField& field,
                  const ::google::protobuf::RepeatedField<bool>& values) {
    arrow::BooleanBuilder array_builder;
    SERVING_CHECK_ARROW_STATUS(
        array_builder.AppendValues(values.begin(), values.end()));
    SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
  }
  void operator()(const FeatureField& field,
                  const ::google::protobuf::RepeatedField<int32_t>& values) {
    switch (target_field->type()->id()) {
      case arrow::Type::INT8: {
        arrow::Int8Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, std::numeric_limits<int8_t>::min());
          SERVING_ENFORCE_LE(v, std::numeric_limits<int8_t>::max());
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT8: {
        arrow::UInt8Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, 0);
          SERVING_ENFORCE_LE(v, std::numeric_limits<uint8_t>::max());
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::INT16: {
        arrow::Int16Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, std::numeric_limits<int16_t>::min());
          SERVING_ENFORCE_LE(v, std::numeric_limits<int16_t>::max());
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT16: {
        arrow::UInt16Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, 0);
          SERVING_ENFORCE_LE(v, std::numeric_limits<uint16_t>::max());
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::INT32: {
        arrow::Int32Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(
            array_builder.AppendValues(values.begin(), values.end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "{} mismatch types, expect:{}, actual:{}", field.name(),
                      FieldType_Name(DataTypeToFieldType(target_field->type())),
                      FieldType_Name(field.type()));
    }
  }
  void operator()(const FeatureField& field,
                  const ::google::protobuf::RepeatedField<int64_t>& values) {
    switch (target_field->type()->id()) {
      case arrow::Type::UINT32: {
        arrow::UInt32Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, 0);
          SERVING_ENFORCE_LE(v, std::numeric_limits<uint32_t>::max());
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::INT64: {
        arrow::Int64Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(
            array_builder.AppendValues(values.begin(), values.end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::UINT64: {
        arrow::UInt64Builder array_builder;
        SERVING_CHECK_ARROW_STATUS(array_builder.Resize(values.size()));
        for (const auto& v : values) {
          SERVING_ENFORCE_GE(v, 0);
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(v));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "{} mismatch types, expect:{}, actual:{}", field.name(),
                      FieldType_Name(DataTypeToFieldType(target_field->type())),
                      FieldType_Name(field.type()));
    }
  }
  void operator()(const FeatureField& field,
                  const ::google::protobuf::RepeatedField<float>& values) {
    switch (target_field->type()->id()) {
      case arrow::Type::HALF_FLOAT: {
        // currently `half_float` is not completely supported.
        // see `https://arrow.apache.org/docs/12.0/status.html`
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "float16(halffloat) is unsupported.");
        break;
      }
      case arrow::Type::FLOAT: {
        arrow::FloatBuilder array_builder;
        SERVING_CHECK_ARROW_STATUS(
            array_builder.AppendValues(values.begin(), values.end()));
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "{} mismatch types, expect:{}, actual:{}", field.name(),
                      FieldType_Name(DataTypeToFieldType(target_field->type())),
                      FieldType_Name(field.type()));
    }
  }
  void operator()(const FeatureField& field,
                  const ::google::protobuf::RepeatedField<double>& values) {
    SERVING_ENFORCE(target_field->type()->id() == arrow::Type::DOUBLE,
                    errors::INVALID_ARGUMENT,
                    "{} mismatch types, expect:{}, actual:{}", field.name(),
                    FieldType_Name(DataTypeToFieldType(target_field->type())),
                    FieldType_Name(field.type()));
    arrow::DoubleBuilder array_builder;
    SERVING_CHECK_ARROW_STATUS(
        array_builder.AppendValues(values.begin(), values.end()));
    SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
  }
  void operator()(
      const FeatureField& field,
      const ::google::protobuf::RepeatedPtrField<std::string>& values) {
    switch (target_field->type()->id()) {
      case arrow::Type::STRING: {
        arrow::StringBuilder array_builder;
        for (const auto& s : values) {
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(s));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      case arrow::Type::BINARY: {
        arrow::BinaryBuilder array_builder;
        for (const auto& s : values) {
          SERVING_CHECK_ARROW_STATUS(array_builder.Append(s));
        }
        SERVING_CHECK_ARROW_STATUS(array_builder.Finish(&array));
        break;
      }
      default:
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "{} mismatch types, expect:{}, actual:{}", field.name(),
                      FieldType_Name(DataTypeToFieldType(target_field->type())),
                      FieldType_Name(field.type()));
    }
  }

  std::shared_ptr<arrow::Field> target_field;
  std::shared_ptr<arrow::Array> array;
};

}  // namespace

std::shared_ptr<arrow::RecordBatch> FeaturesToRecordBatch(
    const ::google::protobuf::RepeatedPtrField<Feature>& features,
    const std::shared_ptr<const arrow::Schema>& target_schema) {
  arrow::SchemaBuilder schema_builder;
  std::vector<std::shared_ptr<arrow::Array>> arrays;
  int num_rows = -1;

  for (const auto& field : target_schema->fields()) {
    bool found = false;
    for (const auto& f : features) {
      if (f.field().name() == field->name()) {
        FeatureToArrayVisitor visitor{.target_field = field, .array = {}};
        FeatureVisit(visitor, f);

        if (num_rows >= 0) {
          SERVING_ENFORCE_EQ(
              num_rows, visitor.array->length(),
              "features must have same length value. {}:{}, others:{}",
              f.field().name(), visitor.array->length(), num_rows);
        }
        num_rows = visitor.array->length();
        arrays.emplace_back(visitor.array);
        found = true;
        break;
      }
    }
    SERVING_ENFORCE(found, errors::ErrorCode::UNEXPECTED_ERROR,
                    "can not found feature:{} in response", field->name());
  }
  return MakeRecordBatch(target_schema, num_rows, std::move(arrays));
}

std::string SerializeRecordBatch(
    std::shared_ptr<arrow::RecordBatch>& record_batch) {
  std::shared_ptr<arrow::io::BufferOutputStream> out_stream;
  SERVING_GET_ARROW_RESULT(arrow::io::BufferOutputStream::Create(), out_stream);

  std::shared_ptr<arrow::ipc::RecordBatchWriter> writer;
  SERVING_GET_ARROW_RESULT(
      arrow::ipc::MakeStreamWriter(out_stream, record_batch->schema()), writer);

  SERVING_CHECK_ARROW_STATUS(writer->WriteRecordBatch(*record_batch));
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

std::shared_ptr<arrow::Schema> DeserializeSchema(const std::string& buf) {
  std::shared_ptr<arrow::Schema> result;

  std::shared_ptr<arrow::io::RandomAccessFile> buffer_reader =
      std::make_shared<arrow::io::BufferReader>(buf);

  arrow::ipc::DictionaryMemo tmp_memo;
  SERVING_GET_ARROW_RESULT(
      arrow::ipc::ReadSchema(
          std::static_pointer_cast<arrow::io::InputStream>(buffer_reader).get(),
          &tmp_memo),
      result);

  return result;
}

std::shared_ptr<arrow::DataType> FieldTypeToDataType(FieldType field_type) {
  const static std::unordered_map<FieldType, std::shared_ptr<arrow::DataType>>
      kTypeMap = {
          {FieldType::FIELD_BOOL, arrow::boolean()},
          {FieldType::FIELD_INT32, arrow::int32()},
          {FieldType::FIELD_INT64, arrow::int64()},
          {FieldType::FIELD_FLOAT, arrow::float32()},
          {FieldType::FIELD_DOUBLE, arrow::float64()},
          {FieldType::FIELD_STRING, arrow::utf8()},
      };

  auto it = kTypeMap.find(field_type);
  SERVING_ENFORCE(it != kTypeMap.end(), errors::ErrorCode::LOGIC_ERROR,
                  "unsupported arrow data type: {}",
                  FieldType_Name(field_type));
  return it->second;
}

FieldType DataTypeToFieldType(
    const std::shared_ptr<arrow::DataType>& data_type) {
  const static std::unordered_map<arrow::Type::type, FieldType> kFieldTypeMap =
      {
          // supported data_type list:
          // `secretflow_serving/protos/data_type.proto`
          {arrow::Type::type::BOOL, FieldType::FIELD_BOOL},
          {arrow::Type::type::UINT8, FieldType::FIELD_INT32},
          {arrow::Type::type::INT8, FieldType::FIELD_INT32},
          {arrow::Type::type::UINT16, FieldType::FIELD_INT32},
          {arrow::Type::type::INT16, FieldType::FIELD_INT32},
          {arrow::Type::type::INT32, FieldType::FIELD_INT32},
          {arrow::Type::type::UINT32, FieldType::FIELD_INT64},
          {arrow::Type::type::UINT64, FieldType::FIELD_INT64},
          {arrow::Type::type::INT64, FieldType::FIELD_INT64},
          // currently `half_float` is not completely supported.
          // see `https://arrow.apache.org/docs/12.0/status.html`
          // {arrow::Type::type::HALF_FLOAT, FieldType::FIELD_FLOAT},
          {arrow::Type::type::FLOAT, FieldType::FIELD_FLOAT},
          {arrow::Type::type::DOUBLE, FieldType::FIELD_DOUBLE},
          {arrow::Type::type::STRING, FieldType::FIELD_STRING},
          {arrow::Type::type::BINARY, FieldType::FIELD_STRING},
      };

  auto it = kFieldTypeMap.find(data_type->id());
  SERVING_ENFORCE(it != kFieldTypeMap.end(), errors::ErrorCode::LOGIC_ERROR,
                  "unsupported arrow data type: {}",
                  arrow::internal::ToString(data_type->id()));
  return it->second;
}

std::shared_ptr<arrow::DataType> DataTypeToArrowDataType(DataType data_type) {
  const static std::unordered_map<DataType, std::shared_ptr<arrow::DataType>>
      kDataTypeMap = {
          {DT_BOOL, arrow::boolean()},
          {DT_UINT8, arrow::uint8()},
          {DT_INT8, arrow::int8()},
          {DT_UINT16, arrow::uint16()},
          {DT_INT16, arrow::int16()},
          {DT_INT32, arrow::int32()},
          {DT_UINT32, arrow::uint32()},
          {DT_UINT64, arrow::uint64()},
          {DT_INT64, arrow::int64()},
          // currently `half_float` is not completely supported.
          // see `https://arrow.apache.org/docs/12.0/status.html`
          // {DT_FLOAT16, arrow::float16()},
          {DT_FLOAT, arrow::float32()},
          {DT_DOUBLE, arrow::float64()},
          {DT_STRING, arrow::utf8()},
          {DT_BINARY, arrow::binary()},
      };

  auto it = kDataTypeMap.find(data_type);
  SERVING_ENFORCE(it != kDataTypeMap.end(), errors::ErrorCode::LOGIC_ERROR,
                  "unsupported data type: {}", DataType_Name(data_type));
  return it->second;
}

std::shared_ptr<arrow::DataType> DataTypeToArrowDataType(
    const std::string& data_type) {
  DataType d_type;
  SERVING_ENFORCE(DataType_Parse(data_type, &d_type),
                  errors::ErrorCode::UNEXPECTED_ERROR, "unknown data type: {}",
                  data_type);
  return DataTypeToArrowDataType(d_type);
}

void CheckReferenceFields(const std::shared_ptr<arrow::Schema>& src,
                          const std::shared_ptr<arrow::Schema>& dst,
                          const std::string& additional_msg) {
  SERVING_CHECK_ARROW_STATUS(
      src->CanReferenceFieldsByNames(dst->field_names()));
  for (const auto& dst_f : dst->fields()) {
    auto src_f = src->GetFieldByName(dst_f->name());
    SERVING_ENFORCE(
        src_f->type()->id() == dst_f->type()->id(), errors::LOGIC_ERROR,
        "{}. field: {} type not match, expect: {}, get: {}", additional_msg,
        dst_f->name(), dst_f->type()->ToString(), src_f->type()->ToString());
  }
}

arrow::Datum GetRowsFilter(
    const std::shared_ptr<arrow::ChunkedArray>& id_column,
    const std::vector<std::string>& ids) {
  arrow::StringBuilder builder;
  SERVING_CHECK_ARROW_STATUS(builder.AppendValues(ids));
  std::shared_ptr<arrow::Array> query_data_array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&query_data_array));
  {
    arrow::Datum is_in;
    SERVING_GET_ARROW_RESULT(arrow::compute::IsIn(query_data_array, id_column),
                             is_in);
    const auto is_in_array =
        std::static_pointer_cast<arrow::BooleanArray>(is_in.make_array());
    SERVING_ENFORCE(is_in_array->true_count() == is_in_array->length(),
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "query data row ids:{} do not all exists in id column of "
                    "csv file, match count: {}.",
                    fmt::join(ids, ","), is_in_array->true_count());
  }

  // filter query datas
  arrow::Datum filter;
  SERVING_GET_ARROW_RESULT(arrow::compute::IsIn(id_column, query_data_array),
                           filter);
  return filter;
}

std::shared_ptr<arrow::RecordBatch> ExtractRowsFromTable(
    const std::shared_ptr<arrow::Table>& table, const arrow::Datum& filter) {
  arrow::Datum filtered_table;
  SERVING_GET_ARROW_RESULT(arrow::compute::Filter(table, filter),
                           filtered_table);

  std::shared_ptr<arrow::RecordBatch> result;
  SERVING_GET_ARROW_RESULT(filtered_table.table()->CombineChunksToBatch(),
                           result);

  return result;
}

std::shared_ptr<arrow::DoubleArray> CastToDoubleArray(
    const std::shared_ptr<arrow::Array>& array) {
  std::shared_ptr<arrow::DoubleArray> result;
  if (array->type_id() != arrow::Type::DOUBLE) {
    arrow::Datum double_array_datum;
    SERVING_GET_ARROW_RESULT(
        arrow::compute::Cast(
            array, arrow::compute::CastOptions::Safe(arrow::float64())),
        double_array_datum);
    result = std::static_pointer_cast<arrow::DoubleArray>(
        std::move(double_array_datum).make_array());
  } else {
    result = std::static_pointer_cast<arrow::DoubleArray>(array);
  }

  return result;
}

std::shared_ptr<arrow::RecordBatch> ShuffleRecordBatch(
    const std::shared_ptr<arrow::RecordBatch>& input_batch) {
  auto fields = input_batch->schema()->fields();

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(fields.begin(), fields.end(), g);

  std::vector<std::shared_ptr<arrow::Array>> new_columns;
  new_columns.reserve(fields.size());
  for (const auto& f : fields) {
    new_columns.emplace_back(
        input_batch->column(input_batch->schema()->GetFieldIndex(f->name())));
  }

  return arrow::RecordBatch::Make(arrow::schema(fields),
                                  input_batch->num_rows(), new_columns);
}

}  // namespace secretflow::serving
