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

Propagator::Propagator(
    const std::unordered_map<std::string, std::shared_ptr<Node>>& nodes) {
  for (const auto& [node_name, node] : nodes) {
    auto frame = std::make_unique<FrameState>();
    frame->pending_count = node->GetInputNum();
    frame->compute_ctx.inputs.resize(frame->pending_count);

    SERVING_ENFORCE(node_frame_map_.emplace(node_name, std::move(frame)).second,
                    errors::ErrorCode::LOGIC_ERROR);
  }
}

FrameState* Propagator::GetFrame(const std::string& node_name) {
  auto iter = node_frame_map_.find(node_name);
  SERVING_ENFORCE(iter != node_frame_map_.end(), errors::ErrorCode::LOGIC_ERROR,
                  "can not found frame for node: {}", node_name);
  return iter->second.get();
}

}  // namespace secretflow::serving
