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

#include <string>
#include <vector>

#include "arrow/api.h"
#include "arrow/dataset/api.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/ops/node.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving::op {

// two level index:
// first for input edges of this node
// second for multiple parties to this op
using OpComputeInputs =
    std::vector<std::vector<std::shared_ptr<arrow::RecordBatch>>>;

struct OpKernelOptions {
  const NodeDef node_def;
  const std::shared_ptr<OpDef> op_def;
};

struct ComputeContext {
  // TODO: Session
  OpComputeInputs inputs;
  std::shared_ptr<arrow::RecordBatch> output;
};

class OpKernel {
 public:
  explicit OpKernel(OpKernelOptions opts) : opts_(std::move(opts)) {}
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

  void Compute(ComputeContext* ctx) {
    int64_t rows = ctx->inputs.front().front()->num_rows();
    SERVING_ENFORCE_EQ(ctx->inputs.size(), input_schema_list_.size(),
                       "schema size be equal to input edges");

    for (size_t edge_index = 0; edge_index != ctx->inputs.size();
         ++edge_index) {
      auto& edge_inputs = ctx->inputs[edge_index];
      for (size_t party_index = 0; party_index != edge_inputs.size();
           ++party_index) {
        auto& input_table = edge_inputs[party_index];
        SERVING_ENFORCE_EQ(rows, input_table->num_rows(),
                           "rows of all inputs tables should be equal");

        if (!input_table->schema()->Equals(input_schema_list_[edge_index])) {
          // reshape real input base on kernel input_schema
          std::vector<std::shared_ptr<arrow::Array>> sorted_arrays;
          for (int i = 0; i < input_schema_list_[edge_index]->num_fields();
               ++i) {
            auto array_index = input_table->schema()->GetFieldIndex(
                input_schema_list_[edge_index]->field(i)->name());
            SERVING_ENFORCE_GE(array_index, 0);
            sorted_arrays.emplace_back(input_table->column(array_index));
          }
          edge_inputs[party_index] = MakeRecordBatch(
              input_schema_list_[edge_index], rows, sorted_arrays);
        }
      }
    }

    DoCompute(ctx);

    SERVING_ENFORCE_EQ(rows, ctx->output->num_rows(),
                       "rows of input and output be equal");
    SERVING_ENFORCE(ctx->output->schema()->Equals(output_schema_),
                    errors::ErrorCode::LOGIC_ERROR,
                    "schema of output ({}) should match output_schema ({})",
                    ctx->output->schema()->ToString(),
                    output_schema_->ToString());
  }

  virtual void DoCompute(ComputeContext* ctx) = 0;

 protected:
  virtual void BuildInputSchema() = 0;

  virtual void BuildOutputSchema() = 0;

 protected:
  OpKernelOptions opts_;

  std::vector<std::shared_ptr<arrow::Schema>> input_schema_list_;
  std::shared_ptr<arrow::Schema> output_schema_;
};

}  // namespace secretflow::serving::op
