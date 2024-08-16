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

#include "secretflow_serving/framework/model_loader.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <utility>

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/sys_util.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace {

const std::string kManifestFileName = "MANIFEST";

}  // namespace

void ModelLoader::Load(const std::string& file_path) {
  SPDLOG_INFO("begin load file: {}", file_path);

  auto model_dir =
      std::filesystem::path(file_path).parent_path().append("data");
  if (std::filesystem::exists(model_dir)) {
    // remove tmp model dir
    SPDLOG_WARN("remove tmp model dir: {}", model_dir.string());
    std::filesystem::remove_all(model_dir);
  }

  // unzip package file
  try {
    SysUtil::ExtractGzippedArchive(file_path, model_dir);
  } catch (const std::exception& e) {
    std::filesystem::remove_all(file_path);
    SERVING_THROW(errors::ErrorCode::IO_ERROR,
                  "failed to extract model package {}, detail: {}", file_path,
                  e.what());
  }

  auto manifest_path =
      std::filesystem::path(model_dir).append(kManifestFileName);
  SERVING_ENFORCE(
      std::filesystem::exists(manifest_path), errors::ErrorCode::IO_ERROR,
      "can not find manifest file {}, model package file is corrupted",
      manifest_path.string());

  // load manifest
  ModelManifest manifest;
  LoadPbFromJsonFile(manifest_path.string(), &manifest);

  auto model_file_path = model_dir.append(manifest.bundle_path());

  auto model_bundle = std::make_shared<ModelBundle>();
  if (manifest.bundle_format() == FileFormatType::FF_PB) {
    LoadPbFromBinaryFile(model_file_path.string(), model_bundle.get());
  } else if (manifest.bundle_format() == FileFormatType::FF_JSON) {
    LoadPbFromJsonFile(model_file_path.string(), model_bundle.get());
  } else {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "found unknown bundle_format:{}",
                  FileFormatType_Name(manifest.bundle_format()));
  }
  model_bundle_ = std::move(model_bundle);

  SPDLOG_INFO("end load model bundle, name: {}, desc: {}, graph version: {}",
              model_bundle_->name(), model_bundle_->desc(),
              model_bundle_->graph().version());
}

}  // namespace secretflow::serving
