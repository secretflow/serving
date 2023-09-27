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

#include "secretflow_serving/framework/propagator.h"

namespace secretflow::serving {

Propagator::Propagator() {}

FrameState* Propagator::CreateFrame(const std::shared_ptr<Node>& node) {
  auto frame = std::make_unique<FrameState>();
  frame->node_name = node->node_def().name();
  frame->pending_count = node->GetInputNum();
  frame->compute_ctx.inputs =
      std::make_shared<op::OpComputeInputs>(frame->pending_count);

  auto result = frame.get();
  std::lock_guard<std::mutex> lock(mutex_);
  SERVING_ENFORCE(
      node_frame_map_.emplace(node->node_def().name(), std::move(frame)).second,
      errors::ErrorCode::LOGIC_ERROR);

  return result;
}

FrameState* Propagator::FindOrCreateChildFrame(
    FrameState* frame, const std::shared_ptr<Node>& child_node) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = node_frame_map_.find(child_node->node_def().name());
  if (iter != node_frame_map_.end()) {
    return iter->second.get();
  } else {
    auto child_frame = std::make_unique<FrameState>();
    child_frame->node_name = child_node->node_def().name();
    child_frame->parent_name = frame->node_name;
    child_frame->pending_count = child_node->GetInputNum();
    child_frame->compute_ctx.inputs =
        std::make_shared<op::OpComputeInputs>(child_node->GetInputNum());

    auto result = child_frame.get();
    node_frame_map_.emplace(child_node->node_def().name(),
                            std::move(child_frame));
    return result;
  }
}

FrameState* Propagator::GetFrame(const std::string& node_name) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto iter = node_frame_map_.find(node_name);
  SERVING_ENFORCE(iter != node_frame_map_.end(), errors::ErrorCode::LOGIC_ERROR,
                  "can not found frame for node: {}", node_name);
  return iter->second.get();
}

}  // namespace secretflow::serving
