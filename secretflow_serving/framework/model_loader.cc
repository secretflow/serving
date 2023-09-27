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

#include <filesystem>
#include <memory>

#include "spdlog/spdlog.h"

#include "secretflow_serving/framework/executable_impl.h"
#include "secretflow_serving/framework/executor.h"
#include "secretflow_serving/framework/predictor_impl.h"
#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/util/sys_util.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/protos/bundle.pb.h"

namespace secretflow::serving {

namespace {
const std::string kManifestFileName = "MANIFEST";
}

ModelLoader::ModelLoader(const Options& opts,
                         std::shared_ptr<PartyChannelMap> channels)
    : Loader(opts), channels_(channels) {}

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

  ModelBundle model_pb;
  if (manifest.bundle_format() == FileFormatType::FF_PB) {
    LoadPbFromBinaryFile(model_file_path.string(), &model_pb);
  } else if (manifest.bundle_format() == FileFormatType::FF_JSON) {
    LoadPbFromJsonFile(model_file_path.string(), &model_pb);
  } else {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "found unkonwn bundle_format:{}",
                  FileFormatType_Name(manifest.bundle_format()));
  }

  SPDLOG_INFO("load model bundle:{} desc:{} graph version:{}", model_pb.name(),
              model_pb.desc(), model_pb.graph().version());

  auto graph = std::make_unique<Graph>(model_pb.graph());
  const auto& executions = graph->GetExecutions();

  std::vector<std::shared_ptr<Executor>> executors;
  for (const auto& e : executions) {
    executors.emplace_back(std::make_shared<Executor>(e));
  }
  executable_ = std::make_shared<ExecutableImpl>(std::move(executors));

  Predictor::Options predictor_opts;
  predictor_opts.party_id = opts_.party_id;
  predictor_opts.channels = channels_;
  predictor_opts.executions = executions;
  predictor_ = std::make_shared<PredictorImpl>(std::move(predictor_opts));
}

}  // namespace secretflow::serving
