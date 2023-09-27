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

#include "secretflow_serving/util/utils.h"

#include <fstream>
#include <streambuf>

#include "google/protobuf/util/json_util.h"
#include "spdlog/spdlog.h"

namespace secretflow::serving {

void LoadPbFromJsonFile(const std::string& file,
                        ::google::protobuf::Message* message) {
  std::ifstream file_is(file);
  SERVING_ENFORCE(file_is.good(), errors::ErrorCode::FS_INVALID_ARGUMENT,
                  "open failed, file: {}", file);
  std::string content((std::istreambuf_iterator<char>(file_is)),
                      std::istreambuf_iterator<char>());
  JsonToPb(content, message);
}

void LoadPbFromBinaryFile(const std::string& file,
                          ::google::protobuf::Message* message) {
  std::ifstream file_is(file);
  std::string content((std::istreambuf_iterator<char>(file_is)),
                      std::istreambuf_iterator<char>());

  SERVING_ENFORCE(message->ParseFromString(content),
                  errors::ErrorCode::DESERIALIZE_FAILD,
                  "parse pb failed, file: {}", file);
}

void JsonToPb(const std::string& json, ::google::protobuf::Message* message) {
  auto status = ::google::protobuf::util::JsonStringToMessage(json, message);
  if (!status.ok()) {
    SPDLOG_ERROR("json to pb faied, msg:{}, json:{}", status.ToString(), json);
    SERVING_THROW(errors::ErrorCode::DESERIALIZE_FAILD,
                  "json to pb failed, msg:{}", status.ToString());
  }
}

}  // namespace secretflow::serving
