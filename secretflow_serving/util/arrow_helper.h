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

#include "arrow/api.h"
#include "google/protobuf/repeated_field.h"

#include "secretflow_serving/core/exception.h"

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
      value = __r__.ValueOrDie();                        \
    }                                                    \
  } while (false)

inline std::shared_ptr<arrow::RecordBatch> MakeRecordBatch(
    std::shared_ptr<arrow::Schema> schema, int64_t num_rows,
    std::vector<std::shared_ptr<arrow::Array>> columns) {
  auto record_batch =
      arrow::RecordBatch::Make(schema, num_rows, std::move(columns));
  SERVING_CHECK_ARROW_STATUS(record_batch->Validate());
  return record_batch;
}

std::string SerializeRecordBatch(
    std::shared_ptr<arrow::RecordBatch>& recordBatch);

std::shared_ptr<arrow::RecordBatch> DeserializeRecordBatch(
    const std::string& buf);

std::shared_ptr<arrow::DataType> FieldTypeToDataType(FieldType field_type);

FieldType DataTypeToFieldType(
    const std::shared_ptr<arrow::DataType>& data_type);

std::shared_ptr<arrow::RecordBatch> FeaturesToTable(
    const ::google::protobuf::RepeatedPtrField<Feature>& features);

}  // namespace secretflow::serving
