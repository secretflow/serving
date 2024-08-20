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

#include "secretflow_serving/framework/executor.h"

namespace secretflow::serving {

class Executable {
 public:
  struct Task {
    size_t id;

    // input
    // `features` or `prev_node_outputs` should be set
    std::shared_ptr<arrow::RecordBatch> features;
    std::unordered_map<std::string,
                       std::vector<std::shared_ptr<arrow::RecordBatch>>>
        prev_node_outputs;

    // output
    std::vector<NodeOutput> outputs;
  };

 public:
  explicit Executable(std::vector<Executor> executors);
  virtual ~Executable() = default;

  virtual void Run(Task& task);

  virtual const std::shared_ptr<const arrow::Schema>& GetInputFeatureSchema();

 private:
  std::vector<Executor> executors_;
};

}  // namespace secretflow::serving
