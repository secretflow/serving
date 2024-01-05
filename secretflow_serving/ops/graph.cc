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

#include "secretflow_serving/ops/graph.h"

#include <deque>
#include <unordered_map>

#include "fmt/format.h"

#include "secretflow_serving/ops/op_kernel_factory.h"

namespace secretflow::serving {

namespace {

// BFS, out_node ---> in_node
void NodeTraversal(
    std::unordered_map<std::string, std::shared_ptr<Node>>* visited,
    const std::unordered_map<std::string, std::shared_ptr<Node>>& nodes) {
  std::deque<std::shared_ptr<Node>> queue;
  std::unordered_set<std::shared_ptr<Edge>> visited_edges;
  for (const auto& pair : *visited) {
    queue.push_back(pair.second);
  }

  while (!queue.empty()) {
    auto n = queue.front();
    queue.pop_front();
    const auto& in_edges = n->in_edges();
    for (const auto& e : in_edges) {
      auto iter = nodes.find(e->src_node());
      if (iter == nodes.end()) {
        continue;
      }
      const auto& in = iter->second;
      SERVING_ENFORCE(visited_edges.emplace(e).second,
                      errors::ErrorCode::LOGIC_ERROR, "found cycle in graph.");
      if (visited->emplace(in->GetName(), in).second) {
        queue.push_back(in);
      }
    }
  }
}

}  // namespace

Execution::Execution(
    size_t id, ExecutionDef execution_def,
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes)
    : id_(id),
      execution_def_(std::move(execution_def)),
      nodes_(std::move(nodes)),
      is_entry_(false),
      is_exit_(false) {
  // get execution exit nodes & entry nodes
  for (const auto& [node_name, node] : nodes_) {
    const auto& dst_edges = node->out_edges();
    const auto& in_edges = node->in_edges();
    // find exit nodes
    if (dst_edges.empty()) {
      exit_node_names_.emplace(node_name);
      is_exit_ = true;
    } else {
      if (std::any_of(dst_edges.begin(), dst_edges.end(),
                      [&](const auto& edge) {
                        return nodes_.find(edge->dst_node()) == nodes_.end();
                      })) {
        exit_node_names_.emplace(node_name);
      }
    }
    // find entry nodes
    if (in_edges.empty()) {
      entry_nodes_.emplace_back(node);
      is_entry_ = true;
    } else {
      for (const auto& edge : in_edges) {
        if (nodes_.find(edge->src_node()) == nodes_.end()) {
          entry_nodes_.emplace_back(node);
        }
      }
    }
  }

  CheckNodesReachability();
}

DispatchType Execution::GetDispatchType() const {
  return execution_def_.config().dispatch_type();
}

size_t Execution::GetEntryNodeNum() const { return entry_nodes_.size(); }

size_t Execution::GetExitNodeNum() const { return exit_node_names_.size(); }

bool Execution::IsExitNode(const std::string& node_name) const {
  return exit_node_names_.find(node_name) != exit_node_names_.end();
}

const std::shared_ptr<Node>& Execution::GetNode(const std::string& name) const {
  auto iter = nodes_.find(name);
  SERVING_ENFORCE(iter != nodes_.end(), errors::ErrorCode::LOGIC_ERROR,
                  "can not find {} in execution {}", name, id_);
  return iter->second;
}

bool Execution::TryGetNode(const std::string& name,
                           std::shared_ptr<Node>* node) const {
  auto iter = nodes_.find(name);
  if (iter == nodes_.end()) {
    return false;
  }
  *node = iter->second;
  return true;
}

void Execution::CheckNodesReachability() {
  std::unordered_map<std::string, std::shared_ptr<Node>> reachable_nodes;
  for (const auto& n : exit_node_names_) {
    reachable_nodes.emplace(n, nodes_.find(n)->second);
  }

  NodeTraversal(&reachable_nodes, nodes_);

  std::vector<std::string> unreachable_node_names;
  for (const auto& n : nodes_) {
    if (reachable_nodes.find(n.first) == reachable_nodes.end()) {
      unreachable_node_names.emplace_back(n.first);
    }
  }
  SERVING_ENFORCE(unreachable_node_names.empty(),
                  errors::ErrorCode::LOGIC_ERROR,
                  "found unreachable nodes in execution, node name: {}",
                  fmt::join(unreachable_node_names.begin(),
                            unreachable_node_names.end(), ","));
}

Graph::Graph(GraphDef graph_def) : def_(std::move(graph_def)) {
  // TODO: check version

  // TODO: consider not storing def_ to avoiding multiple copies of node_defs
  // and execution_defs

  graph_view_.set_version(def_.version());
  for (auto& node : def_.node_list()) {
    NodeView view;
    *(view.mutable_name()) = node.name();
    *(view.mutable_op()) = node.op();
    *(view.mutable_op_version()) = node.op_version();
    *(view.mutable_parents()) = node.parents();
    graph_view_.mutable_node_list()->Add(std::move(view));
  }
  *(graph_view_.mutable_execution_list()) = def_.execution_list();

  // create nodes
  for (int i = 0; i < def_.node_list_size(); ++i) {
    const auto node_name = def_.node_list(i).name();
    auto node = std::make_shared<Node>(def_.node_list(i));
    SERVING_ENFORCE(nodes_.emplace(node_name, node).second,
                    errors::ErrorCode::LOGIC_ERROR, "found duplicate node:{}",
                    node_name);
  }

  // create edges
  for (const auto& [name, node] : nodes_) {
    const auto& input_nodes = node->GetInputNodeNames();
    if (input_nodes.empty()) {
      SERVING_ENFORCE(node->GetOpDef()->inputs_size() == 1,
                      errors::ErrorCode::LOGIC_ERROR,
                      "the entry op should only have one input to accept "
                      "the features, node:{}, op:{}",
                      name, node->node_def().op());
      entry_nodes_.emplace_back(node);
    }
    for (size_t i = 0; i < input_nodes.size(); ++i) {
      auto n_iter = nodes_.find(input_nodes[i]);
      SERVING_ENFORCE(n_iter != nodes_.end(), errors::ErrorCode::LOGIC_ERROR,
                      "can not found input node:{} for node:{}", input_nodes[i],
                      name);
      auto edge = std::make_shared<Edge>(n_iter->first, name, i);
      n_iter->second->AddOutEdge(edge);
      node->AddInEdge(edge);
      edges_.emplace_back(edge);
    }
  }

  // find exit node
  size_t exit_node_count = 0;
  for (const auto& pair : nodes_) {
    if (pair.second->out_edges().empty()) {
      exit_node_ = pair.second;
      ++exit_node_count;
    }
  }
  SERVING_ENFORCE(!entry_nodes_.empty(), errors::ErrorCode::LOGIC_ERROR,
                  "can not found any entry node, please check graph def.");
  SERVING_ENFORCE(exit_node_count == 1, errors::ErrorCode::LOGIC_ERROR,
                  "found {} exit nodes, expect only 1 in graph",
                  exit_node_count);
  SERVING_ENFORCE(exit_node_->GetOpDef()->tag().returnable(),
                  errors::ErrorCode::LOGIC_ERROR,
                  "exit node({}) op({}) must returnable", exit_node_->GetName(),
                  exit_node_->GetOpDef()->name());

  CheckNodesReachability();
  CheckEdgeValidate();

  BuildExecution();
  CheckExecutionValidate();
}

void Graph::CheckNodesReachability() {
  std::unordered_map<std::string, std::shared_ptr<Node>> reachable_nodes = {
      {exit_node_->GetName(), exit_node_}};

  NodeTraversal(&reachable_nodes, nodes_);

  if (reachable_nodes.size() != nodes_.size()) {
    std::vector<std::string> unreachable_node_names;
    for (const auto& n : nodes_) {
      if (reachable_nodes.find(n.first) == reachable_nodes.end()) {
        unreachable_node_names.emplace_back(n.first);
      }
    }
    SERVING_ENFORCE(unreachable_node_names.empty(),
                    errors::ErrorCode::LOGIC_ERROR,
                    "found unreachable nodes in graph, node name: {}",
                    fmt::join(unreachable_node_names.begin(),
                              unreachable_node_names.end(), ","));
  }
}

void Graph::CheckEdgeValidate() {
  std::unordered_map<std::string, std::shared_ptr<op::OpKernel>> kernel_map;
  const auto get_kernel_func =
      [&](const std::shared_ptr<Node>& n) -> std::shared_ptr<op::OpKernel> {
    auto iter = kernel_map.find(n->GetName());
    if (iter == kernel_map.end()) {
      op::OpKernelOptions ctx{n->node_def(), n->GetOpDef()};
      auto kernel = op::OpKernelFactory::GetInstance()->Create(std::move(ctx));
      kernel_map.emplace(n->GetName(), kernel);
      return kernel;
    } else {
      return iter->second;
    }
  };

  for (const auto& e : edges_) {
    auto src_kernel = get_kernel_func(nodes_[e->src_node()]);
    auto dst_kernel = get_kernel_func(nodes_[e->dst_node()]);

    const auto& src_schema = src_kernel->GetOutputSchema();
    const auto& dst_schema = dst_kernel->GetInputSchema(e->dst_input_id());

    // Check the dst_schema is a subset of the src_schema
    CheckReferenceFields(
        src_schema, dst_schema,
        fmt::format("edge schema check failed, src: {}, dst: {}", e->src_node(),
                    e->dst_node()));
  }
}

void Graph::BuildExecution() {
  std::unordered_set<std::string> node_name_set;
  const auto& execution_def_list = def_.execution_list();
  SERVING_ENFORCE(!execution_def_list.empty(), errors::ErrorCode::LOGIC_ERROR);
  for (int i = 0; i < execution_def_list.size(); ++i) {
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    for (const auto& n_name : execution_def_list[i].nodes()) {
      auto n_iter = nodes_.find(n_name);
      SERVING_ENFORCE(n_iter != nodes_.end(), errors::ErrorCode::LOGIC_ERROR,
                      "can not find node:{} in node_def, execution index:{}",
                      n_name, i);
      nodes.emplace(n_name, n_iter->second);
      SERVING_ENFORCE(node_name_set.emplace(n_name).second,
                      errors::ErrorCode::LOGIC_ERROR,
                      "found duplicate node:{} in executions", n_name);
    }
    executions_.emplace_back(std::make_shared<Execution>(
        i, execution_def_list[i], std::move(nodes)));
  }
  SERVING_ENFORCE(node_name_set.size() == nodes_.size(),
                  errors::ErrorCode::UNEXPECTED_ERROR,
                  "all nodes must be included in executions");
}

void Graph::CheckExecutionValidate() {
  SERVING_ENFORCE(executions_.size() >= 2, errors::ErrorCode::LOGIC_ERROR,
                  "graph must contain 2 executions at least.");

  auto prev_dispatch_type =
      DispatchType::DispatchType_INT_MAX_SENTINEL_DO_NOT_USE_;

  for (const auto& e : executions_) {
    SERVING_ENFORCE(e->GetDispatchType() != prev_dispatch_type,
                    errors::ErrorCode::LOGIC_ERROR,
                    "The dispatch types of two adjacent executions cannot be "
                    "the same, cur exeution id {}, type: {}",
                    e->id(), DispatchType_Name(e->GetDispatchType()));
    prev_dispatch_type = e->GetDispatchType();

    if (e->IsEntry()) {
      SERVING_ENFORCE(e->GetDispatchType() == DispatchType::DP_ALL,
                      errors::ErrorCode::LOGIC_ERROR);
    }
    if (e->IsExit()) {
      SERVING_ENFORCE(e->GetDispatchType() != DispatchType::DP_ALL,
                      errors::ErrorCode::LOGIC_ERROR);
      SERVING_ENFORCE(e->GetExitNodeNum() == 1, errors::ErrorCode::LOGIC_ERROR);
      SERVING_ENFORCE(e->IsExitNode(exit_node_->GetName()),
                      errors::ErrorCode::LOGIC_ERROR);
    }
  }
}

const std::shared_ptr<Node>& Graph::GetNode(const std::string& name) const {
  auto iter = nodes_.find(name);
  SERVING_ENFORCE(iter != nodes_.end(), errors::ErrorCode::LOGIC_ERROR,
                  "can not find node({}) in graph", name);
  return iter->second;
}

}  // namespace secretflow::serving
