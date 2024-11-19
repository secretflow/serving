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
#include "secretflow_serving/util/he_mgm.h"

namespace secretflow::serving::op::phe_2p {

class PheDotProduct : public OpKernel {
 public:
  explicit PheDotProduct(OpKernelOptions opts);

  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  std::vector<std::string> feature_name_list_;
  std::vector<std::string> feature_type_list_;

  heu_matrix::CMatrix c_w_matrix_;
  heu_phe::Ciphertext c_intercept_;

  std::string offset_col_name_;
  std::string result_col_name_;
  std::string rand_number_col_name_;

  bool no_feature_ = false;
  bool has_intercept_ = false;
};

}  // namespace secretflow::serving::op::phe_2p
