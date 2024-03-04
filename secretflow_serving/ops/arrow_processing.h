
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

#include <functional>

#include "secretflow_serving/ops/op_kernel.h"

#include "secretflow_serving/protos/compute_trace.pb.h"

namespace secretflow::serving::op {

class ArrowProcessing : public OpKernel {
 public:
  explicit ArrowProcessing(OpKernelOptions opts);

  void DoCompute(ComputeContext* ctx) override;

 protected:
  void BuildInputSchema() override;

  void BuildOutputSchema() override;

  std::shared_ptr<arrow::RecordBatch> ReplayCompute(
      const std::shared_ptr<arrow::RecordBatch>& input);

 private:
  compute::ComputeTrace compute_trace_;

  std::string input_schema_bytes_;
  std::string output_schema_bytes_;

  std::map<int, std::unique_ptr<arrow::compute::FunctionOptions>> func_opt_map_;
  std::vector<std::function<void(arrow::Datum&, std::vector<arrow::Datum>&)>>
      func_list_;

  int32_t result_id_ = -1;

  bool dummy_flag_ = false;
};

}  // namespace secretflow::serving::op
