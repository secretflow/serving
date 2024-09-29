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

#include <memory>
#include <vector>

#include "secretflow_serving/protos/graph.pb.h"
#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving {

class Edge;

class Node final {
 public:
  explicit Node(NodeDef node_def);
  ~Node() = default;

  size_t GetInputNum() const;

  const std::string& GetName() const;

  const NodeDef& node_def() const { return node_def_; }

  const std::shared_ptr<op::OpDef>& GetOpDef() const { return op_def_; }

  const std::vector<std::string>& GetInputNodeNames() const {
    return input_nodes_;
  }

  const std::vector<std::shared_ptr<Edge>>& in_edges() const {
    return in_edges_;
  }

  const std::vector<std::shared_ptr<Edge>>& out_edges() const {
    return out_edges_;
  }

  void AddInEdge(const std::shared_ptr<Edge>& in_edge);

  void AddOutEdge(const std::shared_ptr<Edge>& out_edge);

 private:
  const NodeDef node_def_;
  const std::shared_ptr<op::OpDef> op_def_;
  std::vector<std::string> input_nodes_;

  std::vector<std::shared_ptr<Edge>> in_edges_;
  std::vector<std::shared_ptr<Edge>> out_edges_;
};

class Edge final {
 public:
  explicit Edge(const std::string& src_node, const std::string& dst_node,
                int dst_input_id)
      : src_node_(src_node), dst_node_(dst_node), dst_input_id_(dst_input_id) {}
  ~Edge() = default;

  [[nodiscard]] const std::string& src_node() const { return src_node_; }

  [[nodiscard]] const std::string& dst_node() const { return dst_node_; }

  [[nodiscard]] size_t dst_input_id() const { return dst_input_id_; }

 private:
  const std::string src_node_;
  const std::string dst_node_;
  size_t dst_input_id_;
};

}  // namespace secretflow::serving
