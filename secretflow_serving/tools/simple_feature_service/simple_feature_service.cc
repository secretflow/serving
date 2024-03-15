// Copyright 2024 Ant Group Co., Ltd.
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

#include "secretflow_serving/tools/simple_feature_service/simple_feature_service.h"

#include <filesystem>
#include <memory>
#include <unordered_map>

#include "absl/strings/str_split.h"
#include "arrow/buffer.h"
#include "arrow/csv/api.h"
#include "arrow/io/buffered.h"
#include "arrow/io/file.h"
#include "arrow/io/interfaces.h"
#include "arrow/io/memory.h"
#include "arrow/memory_pool.h"
#include "arrow/status.h"
#include "arrow/util/io_util.h"
#include "brpc/server.h"
#include "fmt/format.h"
#include "google/protobuf/repeated_field.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/csv_extractor.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/spis/batch_feature_service.pb.h"
#include "secretflow_serving/spis/error_code.pb.h"

namespace secretflow::serving {

namespace {

template <typename ArrowArrayType, typename ProtoArrayType>
void ArrowArrayToPbArray(ArrowArrayType &&array, ProtoArrayType *out) {
  out->Reserve(array->length());
  for (int64_t i = 0; i != array->length(); ++i) {
    *(out->Add()) = array->Value(i);
  }
}

// array should be read from csv using
// the type corresponding to feature_value,
// otherwise the static_pointer_cast is dangerous.
void ArrayToFeatureValue(std::shared_ptr<arrow::Array> array,
                         FeatureValue *feature_value) {
  auto field_type = DataTypeToFieldType(array->type());

  switch (field_type) {
    case FieldType::FIELD_BOOL: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::BooleanArray>(array),
                          feature_value->mutable_bs());
      break;
    }
    case FieldType::FIELD_INT32: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::Int32Array>(array),
                          feature_value->mutable_i32s());
      break;
    }
    case FieldType::FIELD_INT64: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::Int64Array>(array),
                          feature_value->mutable_i64s());
      break;
    }
    case FieldType::FIELD_DOUBLE: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::DoubleArray>(array),
                          feature_value->mutable_ds());
      break;
    }
    case FieldType::FIELD_FLOAT: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::FloatArray>(array),
                          feature_value->mutable_fs());
      break;
    }
    case FieldType::FIELD_STRING: {
      ArrowArrayToPbArray(std::static_pointer_cast<arrow::StringArray>(array),
                          feature_value->mutable_ss());
      break;
    }
    default:
      SERVING_THROW(errors::ErrorCode::LOGIC_ERROR, "unexpected field type: {}",
                    static_cast<int>(field_type));
  }
}

void RecordBatchToFeatures(
    std::shared_ptr<arrow::RecordBatch> record_batch,
    const ::google::protobuf::RepeatedPtrField<FeatureField> &feature_fields,
    ::google::protobuf::RepeatedPtrField<::secretflow::serving::Feature>
        *features) {
  for (const auto &field : feature_fields) {
    auto column = record_batch->GetColumnByName(field.name());
    SERVING_ENFORCE(column != nullptr, errors::ErrorCode::LOGIC_ERROR,
                    "column name: {} was not found in record_batch(schema: {})",
                    field.name(), record_batch->schema()->ToString());
    SERVING_ENFORCE(field.type() == DataTypeToFieldType(column->type()),
                    errors::ErrorCode::LOGIC_ERROR,
                    "record_batch types {} and field types {} do not match.",
                    column->type()->name(), FieldType_Name(field.type()));
    auto *feature = features->Add();
    feature->mutable_field()->set_name(field.name());
    feature->mutable_field()->set_type(field.type());

    ArrayToFeatureValue(column, feature->mutable_value());
  }
}

}  // namespace

SimpleBatchFeatureService::SimpleBatchFeatureService(std::string filename,
                                                     std::string id_name)
    : extractor_(filename, id_name) {}

void SimpleBatchFeatureService::BatchFetchFeature(
    ::google::protobuf::RpcController *controller,
    const spis::BatchFetchFeatureRequest *request,
    spis::BatchFetchFeatureResponse *response,
    ::google::protobuf::Closure *done) {
  brpc::ClosureGuard done_guard(done);

  SPDLOG_INFO("request received: service_id={}, party_id={}",
              request->model_service_id(), request->party_id());

  try {
    auto rows = extractor_.ExtractRows(request->feature_fields(),
                                       request->param().query_datas());
    SPDLOG_INFO("extract {} rows", rows->num_rows());

    RecordBatchToFeatures(rows, request->feature_fields(),
                          response->mutable_features());

    response->mutable_status()->set_code(spis::ErrorCode::OK);
    response->mutable_status()->set_msg("success");

  } catch (Exception &e) {
    if (e.code() == errors::ErrorCode::NOT_FOUND) {
      response->mutable_status()->set_code(spis::ErrorCode::NOT_FOUND);
    } else if (e.code() == errors::ErrorCode::LOGIC_ERROR) {
      response->mutable_status()->set_code(spis::ErrorCode::INTERNAL_ERROR);
    } else if (e.code() == errors::ErrorCode::INVALID_ARGUMENT) {
      response->mutable_status()->set_code(spis::ErrorCode::INVALID_ARGUMENT);
    } else {
      response->mutable_status()->set_code(spis::ErrorCode::INTERNAL_ERROR);
    }
    response->mutable_status()->set_msg(e.what());
  } catch (...) {
    response->mutable_status()->set_code(spis::ErrorCode::INTERNAL_ERROR);
    response->mutable_status()->set_msg("unknown error");
  }
}

}  // namespace secretflow::serving
