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

#include "secretflow_serving/protos/link_function.pb.h"

namespace secretflow::serving::op {

class MergeY : public OpKernel {
 public:
  explicit MergeY(OpKernelOptions opts);

  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  double yhat_scale_ = 1.0;

  LinkFunctionType link_function_;
  std::string input_col_name_;
  std::string output_col_name_;

  int32_t exp_iters_ = 0;
};

}  // namespace secretflow::serving::op
