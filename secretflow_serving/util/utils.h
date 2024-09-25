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

#include <thread>

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/apis/status.pb.h"
#include "secretflow_serving/protos/feature.pb.h"

namespace secretflow::serving {

inline bool CheckStatusOk(const apis::Status& st) {
  return st.code() == errors::ErrorCode::OK;
}

std::string ReadFileContent(const std::string& file);

void LoadPbFromJsonFile(const std::string& file,
                        ::google::protobuf::Message* message);

void LoadPbFromBinaryFile(const std::string& file,
                          ::google::protobuf::Message* message);

void JsonToPb(const std::string& json, ::google::protobuf::Message* message);

std::string PbToJson(const ::google::protobuf::Message* message);

std::string PbToJsonNoExcept(
    const ::google::protobuf::Message* message) noexcept;

template <typename Func>
void FeatureVisit(Func&& visitor, const Feature& f) {
  switch (f.field().type()) {
    case FieldType::FIELD_BOOL: {
      visitor(f.field(), f.value().bs());
      break;
    }
    case FieldType::FIELD_INT32: {
      visitor(f.field(), f.value().i32s());
      break;
    }
    case FieldType::FIELD_INT64: {
      visitor(f.field(), f.value().i64s());
      break;
    }
    case FieldType::FIELD_FLOAT: {
      visitor(f.field(), f.value().fs());
      break;
    }
    case FieldType::FIELD_DOUBLE: {
      visitor(f.field(), f.value().ds());
      break;
    }
    case FieldType::FIELD_STRING: {
      visitor(f.field(), f.value().ss());
      break;
    }
    default:
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "unkown field type {}",
                    FieldType_Name(f.field().type()));
  }
}

size_t CountSampleNum(
    const ::google::protobuf::RepeatedPtrField<Feature>& features);

std::string UnescapeJson(const std::string& json);

bool CheckContentEmpty(const std::string& str);

class RetryRunner {
 public:
  RetryRunner(uint32_t retry_counts, uint32_t retry_interval_ms)
      : retry_counts_(retry_counts), retry_interval_ms_(retry_interval_ms) {}

  template <typename Func, typename... Args,
            typename = std::enable_if_t<
                std::is_same_v<bool, std::invoke_result_t<Func, Args...>>>>
  bool Run(Func&& f, Args&&... args) const {
    auto runner_func = [&] {
      return std::invoke(std::forward<Func>(f), std::forward<Args>(args)...);
    };
    for (uint32_t i = 0; i != retry_counts_; ++i) {
      if (!runner_func()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(retry_interval_ms_));
      } else {
        return true;
      }
    }
    return false;
  }

 private:
  uint32_t retry_counts_;
  uint32_t retry_interval_ms_;
};
}  // namespace secretflow::serving
