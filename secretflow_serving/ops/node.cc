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

#include "secretflow_serving/ops/node.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/op_factory.h"

namespace secretflow::serving {

Node::Node(NodeDef node_def)
    : node_def_(std::move(node_def)),
      op_def_(op::OpFactory::GetInstance()->Get(node_def_.op())) {
  if (node_def_.parents_size() > 0) {
    // TODO: support node have one const feature input case
    if (node_def_.parents_size() != op_def_->inputs_size()) {
      // check op input is variable
      if (!op_def_->tag().variable_inputs()) {
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "node({}) input size({}) not fit op({}) input size({})",
                      node_def_.name(), node_def_.parents_size(),
                      op_def_->name(), op_def_->inputs_size());
      }
    }
  }
  input_nodes_ = {node_def_.parents().begin(), node_def_.parents().end()};
}

size_t Node::GetInputNum() const { return node_def_.parents_size(); }

const std::string& Node::GetName() const { return node_def_.name(); }

void Node::AddInEdge(const std::shared_ptr<Edge>& in_edge) {
  in_edges_.emplace_back(in_edge);
}

void Node::AddOutEdge(const std::shared_ptr<Edge>& out_edge) {
  out_edges_.emplace_back(out_edge);
}

}  // namespace secretflow::serving
