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

#include "secretflow_serving/framework/executable.h"
#include "secretflow_serving/framework/executor.h"

namespace secretflow::serving {

class ExecutableImpl : public Executable {
 public:
  explicit ExecutableImpl(std::vector<std::shared_ptr<Executor>> executors);
  ~ExecutableImpl() override = default;

  void Run(Task& task) override;

  const std::shared_ptr<const arrow::Schema>& GetInputFeatureSchema() override;

 private:
  std::vector<std::shared_ptr<Executor>> executors_;
};

}  // namespace secretflow::serving
