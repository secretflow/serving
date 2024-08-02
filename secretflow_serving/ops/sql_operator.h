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
#include "secretflow_serving/ops/op_kernel.h"
#include "secretflow_serving/ops/sql/sql_processor.h"

namespace secretflow::serving::op {

class SqlOperator : public OpKernel {
 public:
  explicit SqlOperator(OpKernelOptions opts);

// 执行计算
  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

 private:
  std::string tbl_name_;
  std::vector<std::string> input_feature_names_;
  std::vector<std::string> input_feature_types_;
  std::vector<std::string> output_feature_names_;
  std::vector<std::string> output_feature_types_;
  std::string sql_;
  bool is_compute_run_ = false;
};

}  // namespace secretflow::serving::op
