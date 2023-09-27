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

#include <map>
#include <string>
#include <vector>

#include "arrow/api.h"
#include "arrow/dataset/api.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node.h"

#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving::op {

using OpComputeInputs =
    std::vector<std::vector<std::shared_ptr<arrow::RecordBatch>>>;

struct OpKernelOptions {
  const std::shared_ptr<Node> node;
};

struct ComputeContext {
  // TODO: Session
  std::shared_ptr<OpComputeInputs> inputs;
  std::shared_ptr<arrow::RecordBatch> output;
};

class OpKernel {
 public:
  explicit OpKernel(OpKernelOptions opts) : opts_(std::move(opts)) {
    SPDLOG_INFO("op kernel: {}, version: {}, node: {}",
                opts_.node->GetOpDef()->name(),
                opts_.node->GetOpDef()->version(), opts_.node->GetName());
  }
  virtual ~OpKernel() = default;

  size_t GetInputsNum() const { return input_schema_list_.size(); }

  const std::shared_ptr<arrow::Schema>& GetInputSchema(size_t index) const {
    return input_schema_list_[index];
  }

  const std::vector<std::shared_ptr<arrow::Schema>>& GetAllInputSchema() const {
    return input_schema_list_;
  }

  const std::shared_ptr<arrow::Schema>& GetOutputSchema() const {
    return output_schema_;
  }

  virtual void Compute(ComputeContext* ctx) = 0;

 protected:
  virtual void BuildInputSchema() = 0;

  virtual void BuildOutputSchema() = 0;

 protected:
  OpKernelOptions opts_;

  std::vector<std::shared_ptr<arrow::Schema>> input_schema_list_;
  std::shared_ptr<arrow::Schema> output_schema_;
};

}  // namespace secretflow::serving::op
