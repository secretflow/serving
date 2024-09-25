// Copyright 2024 Ant Group Co., Ltd.
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

namespace secretflow::serving::op::phe_2p {

class PheMergeY : public OpKernel {
 public:
  explicit PheMergeY(OpKernelOptions opts);

  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  std::string score_col_name_;
  std::string decrypted_y_col_name_;
  std::string crypted_y_col_name_;

  double yhat_scale_ = 1.0;
  LinkFunctionType link_function_;
  int32_t exp_iters_ = 0;
};

}  // namespace secretflow::serving::op::phe_2p
