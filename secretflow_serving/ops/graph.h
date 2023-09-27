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

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "arrow/api.h"

#include "secretflow_serving/ops/node.h"

namespace secretflow::serving {

class Execution final {
 public:
  explicit Execution(size_t id, ExecutionDef execution_def,
                     std::map<std::string, std::shared_ptr<Node>> nodes);
  ~Execution() = default;

  size_t id() const { return id_; }

  const ExecutionDef& execution_def() const { return execution_def_; }

  bool IsEntry() const { return is_entry_; }

  bool IsExit() const { return is_exit_; }

  DispatchType GetDispatchType() const;

  size_t GetEntryNodeNum() const;

  size_t GetExitNodeNum() const;

  bool IsExitNode(const std::string& node_name) const;

  const std::vector<std::shared_ptr<Node>>& GetEntryNodes() const {
    return entry_nodes_;
  }

  const std::map<std::string, std::shared_ptr<Node>>& nodes() const {
    return nodes_;
  }

  const std::shared_ptr<Node>& GetNode(const std::string& name) const;

 protected:
  void CheckNodesReachability();

 private:
  const size_t id_;
  const ExecutionDef execution_def_;
  const std::map<std::string, std::shared_ptr<Node>> nodes_;

  bool is_entry_;
  bool is_exit_;

  std::vector<std::shared_ptr<Node>> entry_nodes_;
  std::set<std::string> exit_node_names_;
};

class Graph final {
 public:
  explicit Graph(GraphDef graph_def);
  ~Graph() = default;

  const GraphDef& def() { return def_; }

  const std::vector<std::shared_ptr<Execution>>& GetExecutions() const {
    return executions_;
  }

 protected:
  void CheckNodesReachability();

  void CheckEdgeValidate();

  void BuildExecution();

  void CheckExecutionValidate();

 private:
  const GraphDef def_;

  std::map<std::string, std::shared_ptr<Node>> nodes_;
  std::vector<std::shared_ptr<Edge>> edges_;
  std::vector<std::shared_ptr<Execution>> executions_;

  std::vector<std::shared_ptr<Node>> entry_nodes_;
  std::shared_ptr<Node> exit_node_;
};

}  // namespace secretflow::serving
