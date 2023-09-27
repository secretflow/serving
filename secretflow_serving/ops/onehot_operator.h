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

#include "secretflow_serving/core/types.h"
#include "secretflow_serving/ops/onehot/onehot_substitution.h"
#include "secretflow_serving/ops/op_kernel.h"

namespace secretflow::serving::op {

class OnehotOperator : public OpKernel {
 public:
  explicit OnehotOperator(OpKernelOptions opts);

  void Compute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  std::string rule_json_;  // json
  std::vector<std::string> input_feature_names_;
  std::vector<std::string> input_feature_types_;
  std::vector<std::string> output_feature_names_;
  std::vector<std::string> output_feature_types_;
  std::unique_ptr<secretflow::serving::ops::onehot::OneHotSubstitution>
      onehot_substitution_;
  std::unordered_map<std::string, std::string> output_feature_name_type_map_;
  bool is_compute_run_ = false;
};

}  // namespace secretflow::serving::op