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

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/apis/status.pb.h"

namespace secretflow::serving {

inline bool CheckStatusOk(const apis::Status& st) {
  if (st.code() == errors::ErrorCode::OK) {
    return true;
  } else {
    return false;
  }
}

void LoadPbFromJsonFile(const std::string& file,
                        ::google::protobuf::Message* message);

void LoadPbFromBinaryFile(const std::string& file,
                          ::google::protobuf::Message* message);

void JsonToPb(const std::string& json, ::google::protobuf::Message* message);

}  // namespace secretflow::serving
