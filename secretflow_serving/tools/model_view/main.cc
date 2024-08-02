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

#include <filesystem>
#include <iostream>

#include "gflags/gflags.h"

#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/protos/bundle.pb.h"

namespace secretflow::serving {

static void ShowModel(const ModelBundle& model_pb) {
  auto model_json_content = PbToJson(&model_pb);
  std::cout << "Model content: " << std::endl;
  std::cout << model_json_content << std::endl;

  // get input schema & output schema
  std::cout << std::endl;
  std::cout << "Io schema: " << std::endl;
  Graph graph(model_pb.graph());
  for (const auto& node_def : model_pb.graph().node_list()) {
    const auto& node = graph.GetNode(node_def.name());

    op::OpKernelOptions ctx{node->node_def(), node->GetOpDef()};
    auto op_kernel = op::OpKernelFactory::GetInstance()->Create(std::move(ctx));

    std::cout << "==========================" << std::endl;
    std::cout << "node: " << node_def.name() << std::endl;
    auto inputs_num = op_kernel->GetInputsNum();
    for (size_t i = 0; i < inputs_num; ++i) {
      std::cout << "--------------------------" << std::endl;
      std::cout << i << " input:" << std::endl;
      std::cout << op_kernel->GetInputSchema(i)->ToString() << std::endl;
    }
    std::cout << "--------------------------" << std::endl;
    std::cout << "output:" << std::endl;
    std::cout << op_kernel->GetOutputSchema()->ToString() << std::endl;
  }
}

static void ShowPBModel(const std::string& file_path) {
  ModelBundle model_pb;
  LoadPbFromBinaryFile(file_path, &model_pb);
  ShowModel(model_pb);
}

static void ShowJsonModel(const std::string& file_path) {
  ModelBundle model_pb;
  LoadPbFromJsonFile(file_path, &model_pb);
  ShowModel(model_pb);
}

void ShowModelFile(FileFormatType model_type, const std::string& model_file) {
  if (!std::filesystem::exists(model_file)) {
    throw std::runtime_error("model file does not exist.");
  }

  if (model_type == FileFormatType::FF_PB) {
    ShowPBModel(model_file);

  } else if (model_type == FileFormatType::FF_JSON) {
    ShowJsonModel(model_file);

  } else {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "found unknown bundle_format:{}",
                  FileFormatType_Name(model_type));
  }
}

void ShowModelFile(const std::string& manifest_file,
                   const std::string& model_file) {
  if (!std::filesystem::exists(manifest_file)) {
    throw std::runtime_error("manifest file does not exist.");
  }

  ModelManifest manifest;
  LoadPbFromJsonFile(manifest_file, &manifest);
  ShowModelFile(manifest.bundle_format(), model_file);
}

}  // namespace secretflow::serving

DEFINE_string(model_file, "", "filename of model");
DEFINE_string(manifest, "", "filename of manifest");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_model_file.empty() || FLAGS_manifest.empty()) {
    std::cerr << "Usage: " << argv[0]
              << " --model_file=<model_file> "
                 "--manifest=<manifest_file>"
              << std::endl;
    return -1;
  }

  std::cout << "model_file: " << FLAGS_model_file << std::endl;

  try {
    std::cout << "manifest_file: " << FLAGS_manifest << std::endl;
    secretflow::serving::ShowModelFile(FLAGS_manifest, FLAGS_model_file);

  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  return 0;
}
