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

#include "secretflow_serving/ops/op_kernel.h"
#include "secretflow_serving/ops/tree_utils.h"

namespace secretflow::serving::op {

class TreeSelect : public OpKernel {
 public:
  explicit TreeSelect(OpKernelOptions opts);

  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  std::vector<std::string> feature_name_list_;
  std::vector<std::string> feature_type_list_;

  std::string output_col_name_;

  int32_t root_node_id_;
  std::map<int32_t, TreeNode> nodes_;
  std::set<size_t> used_feature_idx_list_;

  size_t num_leaf_ = 0;
};

}  // namespace secretflow::serving::op
