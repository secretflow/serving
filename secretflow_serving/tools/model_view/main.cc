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

#include <iostream>

#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/protos/bundle.pb.h"

namespace secretflow::serving {

static void ShowModel(const std::string& file_path) {
  ModelBundle model_pb;
  LoadPbFromBinaryFile(file_path, &model_pb);

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

}  // namespace secretflow::serving

const char* help_msg = R"MSG(
Usage: model_view <file>
View file content of secretflow binary format model file.
)MSG";
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << help_msg << std::endl;
    return 1;
  }

  try {
    secretflow::serving::ShowModel(argv[1]);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  return 0;
}
