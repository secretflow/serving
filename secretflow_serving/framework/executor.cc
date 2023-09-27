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

#include "secretflow_serving/framework/executor.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"

namespace secretflow::serving {

Executor::Executor(const std::shared_ptr<Execution>& execution)
    : execution_(execution) {
  // create op_kernel
  auto nodes = execution_->nodes();
  for (const auto& [node_name, node] : nodes) {
    op::OpKernelOptions ctx{node};

    auto item = std::make_shared<NodeItem>();
    item->node = node;
    item->op_kernel =
        op::OpKernelFactory::GetInstance()->Create(std::move(ctx));
    node_items_.emplace(node_name, item);
  }

  // get input schema
  const auto& entry_nodes = execution_->GetEntryNodes();
  for (const auto& node : entry_nodes) {
    const auto& node_name = node->node_def().name();
    auto iter = node_items_.find(node_name);
    SERVING_ENFORCE(iter != node_items_.end(), errors::ErrorCode::LOGIC_ERROR);
    const auto& input_schema = iter->second->op_kernel->GetAllInputSchema();
    entry_node_names_.emplace_back(node_name);
    input_schema_map_.emplace(node_name, input_schema);
  }

  if (execution_->IsEntry()) {
    // build feature schema from entry execution
    auto iter = input_schema_map_.begin();
    const auto& fisrt_input_schema_list = iter->second;
    SERVING_ENFORCE(fisrt_input_schema_list.size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);
    const auto& target_schema = fisrt_input_schema_list.front();
    ++iter;
    for (; iter != input_schema_map_.end(); ++iter) {
      SERVING_ENFORCE(iter->second.size() == 1, errors::ErrorCode::LOGIC_ERROR);
      const auto& schema = iter->second.front();
      SERVING_ENFORCE(schema->Equals(target_schema),
                      errors::ErrorCode::LOGIC_ERROR,
                      "found entry nodes input schema not equals");
      // TODO: consider support entry nodes schema have same fields but
      // different ordered
    }
    input_feature_schema_ = target_schema;
  }
}

std::shared_ptr<std::vector<NodeOutput>> Executor::Run(
    std::shared_ptr<arrow::RecordBatch>& features) {
  SERVING_ENFORCE(execution_->IsEntry(), errors::ErrorCode::LOGIC_ERROR);
  auto inputs = std::make_shared<
      std::map<std::string, std::shared_ptr<op::OpComputeInputs>>>();
  for (size_t i = 0; i < entry_node_names_.size(); ++i) {
    auto op_inputs = std::make_shared<op::OpComputeInputs>();
    std::vector<std::shared_ptr<arrow::RecordBatch>> record_list = {features};
    op_inputs->emplace_back(std::move(record_list));
    inputs->emplace(entry_node_names_[i], std::move(op_inputs));
  }
  return Run(inputs);
}

std::shared_ptr<std::vector<NodeOutput>> Executor::Run(
    std::shared_ptr<
        std::map<std::string, std::shared_ptr<op::OpComputeInputs>>>& inputs) {
  auto results = std::make_shared<std::vector<NodeOutput>>();
  Propagator propagator;

  // get entry nodes
  std::deque<std::shared_ptr<NodeItem>> ready_nodes;
  const auto& entry_nodes = execution_->GetEntryNodes();
  for (const auto& node : entry_nodes) {
    const auto& node_name = node->node_def().name();
    ready_nodes.emplace_back(node_items_.find(node_name)->second);
    auto frame = propagator.CreateFrame(node);
    auto in_iter = inputs->find(node_name);
    SERVING_ENFORCE(in_iter != inputs->end(),
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "can not found inputs for node:{}", node_name);
    frame->compute_ctx.inputs = in_iter->second;
  }

  // schedule ready
  // TODO: support multi-thread run
  size_t scheduled_count = 0;
  while (!ready_nodes.empty()) {
    auto n = ready_nodes.front();
    ready_nodes.pop_front();

    auto frame = propagator.GetFrame(n->node->node_def().name());

    n->op_kernel->Compute(&(frame->compute_ctx));
    ++scheduled_count;

    if (execution_->IsExitNode(n->node->node_def().name())) {
      NodeOutput node_output{n->node->node_def().name(),
                             frame->compute_ctx.output};
      results->emplace_back(std::move(node_output));
    } else {
      auto dst_node_name = n->node->out_edge()->dst_node();
      auto dst_node = execution_->GetNode(dst_node_name);
      auto child_frame = propagator.FindOrCreateChildFrame(frame, dst_node);
      child_frame->compute_ctx.inputs->at(
          n->node->out_edge()->dst_input_id()) = {frame->compute_ctx.output};
      child_frame->pending_count--;
      if (child_frame->pending_count == 0) {
        ready_nodes.push_back(
            node_items_.find(dst_node->node_def().name())->second);
      }
    }
  }

  SERVING_ENFORCE_EQ(scheduled_count, execution_->nodes().size());
  return results;
}

}  // namespace secretflow::serving
