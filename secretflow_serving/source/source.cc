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

#include "secretflow_serving/source/source.h"

#include <filesystem>

#include "spdlog/spdlog.h"

#include "secretflow_serving/util/sys_util.h"

namespace secretflow::serving {

namespace {

const std::string kModelFileName = "model_bundle.tar.gz";

}

Source::Source(const ModelConfig& config, const std::string& service_id)
    : config_(config),
      service_id_(service_id),
      data_dir_(std::filesystem::path(config.base_path())
                    .append(service_id_)
                    .string()) {}

std::string Source::PullModel() {
  auto dst_dir = std::filesystem::path(data_dir_).append(config_.model_id());
  if (!std::filesystem::exists(dst_dir)) {
    std::filesystem::create_directories(dst_dir);
  }

  auto dst_file_path = dst_dir.append(kModelFileName);
  const auto& source_sha256 = config_.source_sha256();
  if (std::filesystem::exists(dst_file_path)) {
    if (!source_sha256.empty()) {
      if (SysUtil::CheckSHA256(dst_file_path.string(), source_sha256)) {
        return dst_file_path;
      }
    }
    SPDLOG_INFO("remove tmp model file:{}", dst_file_path.string());
    std::filesystem::remove(dst_file_path);
  }

  OnPullModel(dst_file_path);
  if (!source_sha256.empty()) {
    SERVING_ENFORCE(SysUtil::CheckSHA256(dst_file_path.string(), source_sha256),
                    errors::ErrorCode::IO_ERROR,
                    "model({}) sha256 check failed", config_.source_path());
  }

  return dst_file_path;
}

}  // namespace secretflow::serving
