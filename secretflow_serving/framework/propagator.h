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

#include <mutex>

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/ops/op_kernel.h"

namespace secretflow::serving {

struct FrameState {
  std::atomic<int> pending_count;

  op::ComputeContext compute_ctx;
};

class Propagator {
 public:
  explicit Propagator(
      const std::unordered_map<std::string, std::shared_ptr<Node>>& nodes);

  FrameState* GetFrame(const std::string& node_name);

 private:
  std::unordered_map<std::string, FrameState*> node_frame_map_;
  std::vector<FrameState> frame_pool_;
};
}  // namespace secretflow::serving
