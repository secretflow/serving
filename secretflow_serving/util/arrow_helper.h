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

#pragma once

#include <utility>

#include "arrow/api.h"
#include "google/protobuf/repeated_field.h"

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/protos/data_type.pb.h"
#include "secretflow_serving/protos/feature.pb.h"

namespace secretflow::serving {

#define SERVING_CHECK_ARROW_STATUS(status)                                 \
  do {                                                                     \
    auto __s__ = (status);                                                 \
    if (!__s__.ok()) {                                                     \
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, __s__.message()); \
    }                                                                      \
  } while (false)

#define SERVING_GET_ARROW_RESULT(result, value)          \
  do {                                                   \
    auto __r__ = (result);                               \
    if (!__r__.ok()) {                                   \
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, \
                    __r__.status().message());           \
    } else {                                             \
      value = std::move(__r__.ValueOrDie());             \
    }                                                    \
  } while (false)

inline std::shared_ptr<arrow::RecordBatch> MakeRecordBatch(
    const std::shared_ptr<arrow::Schema>& schema, int64_t num_rows,
    std::vector<std::shared_ptr<arrow::Array>> columns) {
  auto record_batch =
      arrow::RecordBatch::Make(schema, num_rows, std::move(columns));
  SERVING_CHECK_ARROW_STATUS(record_batch->Validate());
  return record_batch;
}

inline std::shared_ptr<arrow::RecordBatch> MakeRecordBatch(
    const std::shared_ptr<const arrow::Schema>& schema, int64_t num_rows,
    std::vector<std::shared_ptr<arrow::Array>> columns) {
  return MakeRecordBatch(std::const_pointer_cast<arrow::Schema>(schema),
                         num_rows, std::move(columns));
}

std::string SerializeRecordBatch(
    std::shared_ptr<arrow::RecordBatch>& record_batch);

std::shared_ptr<arrow::RecordBatch> DeserializeRecordBatch(
    const std::string& buf);

std::shared_ptr<arrow::Schema> DeserializeSchema(const std::string& buf);

FieldType DataTypeToFieldType(
    const std::shared_ptr<arrow::DataType>& data_type);

std::shared_ptr<arrow::RecordBatch> FeaturesToTable(
    const ::google::protobuf::RepeatedPtrField<Feature>& features,
    const std::shared_ptr<const arrow::Schema>& target_schema);

inline void CheckArrowDataTypeValid(
    const std::shared_ptr<arrow::DataType>& data_type) {
  SERVING_ENFORCE(
      arrow::is_numeric(data_type->id()) || arrow::is_string(data_type->id()) ||
          arrow::is_binary(data_type->id()),
      errors::ErrorCode::LOGIC_ERROR, "unsupported arrow data type: {}",
      arrow::internal::ToString(data_type->id()));
  SERVING_ENFORCE(data_type->id() != arrow::Type::HALF_FLOAT,
                  errors::ErrorCode::LOGIC_ERROR,
                  "float16(halffloat) is unsupported.");
}

std::shared_ptr<arrow::DataType> DataTypeToArrowDataType(DataType data_type);

std::shared_ptr<arrow::DataType> DataTypeToArrowDataType(
    const std::string& data_type);

// Check that all fields in 'dst' can be found in 'src' and that the data type
// of each field is also consistent.
void CheckReferenceFields(const std::shared_ptr<arrow::Schema>& src,
                          const std::shared_ptr<arrow::Schema>& dst,
                          const std::string& additional_msg = "");

}  // namespace secretflow::serving
