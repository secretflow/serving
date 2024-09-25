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

#include "secretflow_serving/framework/executable.h"

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

Executable::Executable(std::vector<Executor> executors)
    : executors_(std::move(executors)) {}

void Executable::Run(Task& task) {
  SERVING_ENFORCE(task.id < executors_.size(), errors::ErrorCode::LOGIC_ERROR);
  auto executor = executors_[task.id];
  if (task.features) {
    task.outputs = executor.Run(task.requester_id, task.features);
  } else {
    SERVING_ENFORCE(!task.prev_node_outputs.empty(),
                    errors::ErrorCode::LOGIC_ERROR);
    task.outputs = executor.Run(task.requester_id, task.prev_node_outputs);
  }

  SPDLOG_DEBUG("Executable::Run end, task.outputs.size:{}",
               task.outputs->size());
}

const std::shared_ptr<const arrow::Schema>&
Executable::GetInputFeatureSchema() {
  const auto& schema = executors_.front().GetInputFeatureSchema();
  SERVING_ENFORCE(schema, errors::ErrorCode::LOGIC_ERROR);
  return schema;
}

}  // namespace secretflow::serving
