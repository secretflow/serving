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

#include <filesystem>
#include <fstream>
#include <streambuf>

#include "google/protobuf/util/json_util.h"
#include "spdlog/spdlog.h"

namespace secretflow::serving {

namespace {
std::string ReadFileContent(const std::string& file) {
  if (!std::filesystem::exists(file)) {
    SERVING_THROW(errors::ErrorCode::IO_ERROR, "can not find file: {}", file);
  }
  std::ifstream file_is(file);
  SERVING_ENFORCE(file_is.good(), errors::ErrorCode::IO_ERROR,
                  "open failed, file: {}", file);
  return std::string((std::istreambuf_iterator<char>(file_is)),
                     std::istreambuf_iterator<char>());
}
}  // namespace

void LoadPbFromJsonFile(const std::string& file,
                        ::google::protobuf::Message* message) {
  JsonToPb(ReadFileContent(file), message);
}

void LoadPbFromBinaryFile(const std::string& file,
                          ::google::protobuf::Message* message) {
  SERVING_ENFORCE(message->ParseFromString(ReadFileContent(file)),
                  errors::ErrorCode::DESERIALIZE_FAILED,
                  "parse pb failed, file: {}", file);
}

void JsonToPb(const std::string& json, ::google::protobuf::Message* message) {
  auto status = ::google::protobuf::util::JsonStringToMessage(json, message);
  if (!status.ok()) {
    SPDLOG_ERROR("json to pb failed, msg:{}, json:{}", status.ToString(), json);
    SERVING_THROW(errors::ErrorCode::DESERIALIZE_FAILED,
                  "json to pb failed, msg:{}", status.ToString());
  }
}

std::string PbToJson(const ::google::protobuf::Message* message) {
  std::string json;
  auto status = ::google::protobuf::util::MessageToJsonString(*message, &json);
  if (!status.ok()) {
    SPDLOG_ERROR("pb to json failed, msg:{}, message:{}", status.ToString(),
                 message->ShortDebugString());
    SERVING_THROW(errors::ErrorCode::SERIALIZE_FAILED,
                  "pb to json failed, msg:{}", status.ToString());
  }
  return json;
}

}  // namespace secretflow::serving
