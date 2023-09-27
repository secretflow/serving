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

#include "secretflow_serving/source/filesystem_source.h"

#include <filesystem>

#include "spdlog/spdlog.h"

#include "secretflow_serving/source/factory.h"

namespace secretflow::serving {

FileSystemSource::FileSystemSource(const ModelConfig& config,
                                   const std::string& service_id)
    : Source(config, service_id) {}

void FileSystemSource::OnPullModel(const std::string& dst_path) {
  // just copy file
  SERVING_ENFORCE(std::filesystem::exists(config_.source_path()),
                  errors::ErrorCode::NOT_FOUND,
                  "source_path {} in model_conf  does not exist",
                  config_.source_path());
  std::filesystem::copy(config_.source_path(), dst_path);

  SPDLOG_INFO("copy model file from {} to {}", config_.source_path(), dst_path);
}

REGISTER_SOURCE(SourceType::ST_FILE, FileSystemSource);

}  // namespace secretflow::serving
