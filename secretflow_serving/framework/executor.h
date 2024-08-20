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

#include "secretflow_serving/framework/propagator.h"
#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/ops/op_kernel.h"

namespace secretflow::serving {

struct NodeOutput {
  std::string node_name;
  std::shared_ptr<arrow::RecordBatch> table;
  NodeOutput(std::string name, std::shared_ptr<arrow::RecordBatch> table)
      : node_name(std::move(name)), table(std::move(table)) {}
};

struct NodeItem {
  std::shared_ptr<Node> node;
  std::shared_ptr<op::OpKernel> op_kernel;
};

class Executor {
 public:
  explicit Executor(const std::shared_ptr<Execution>& execution);
  ~Executor() = default;

  const std::shared_ptr<const arrow::Schema>& GetInputFeatureSchema() const {
    return input_feature_schema_;
  }

  // for middle execution
  std::vector<NodeOutput> Run(
      std::unordered_map<std::string,
                         std::vector<std::shared_ptr<arrow::RecordBatch>>>&
          prev_node_outputs);

  // for entry execution
  std::vector<NodeOutput> Run(std::shared_ptr<arrow::RecordBatch>& features);

 private:
  std::shared_ptr<Execution> execution_;

  std::shared_ptr<std::unordered_map<std::string, NodeItem>> node_items_;

  std::unordered_map<std::string,
                     const std::vector<std::shared_ptr<arrow::Schema>>>
      input_schema_map_;

  std::shared_ptr<const arrow::Schema> input_feature_schema_;
};

}  // namespace secretflow::serving
