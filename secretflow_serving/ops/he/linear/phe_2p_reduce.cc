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

#include "secretflow_serving/ops/he/linear/phe_2p_reduce.h"

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::op::phe_2p {

namespace {

template <typename T>
::yacl::Buffer AddRand(heu_matrix::Evaluator* evaluator, std::string_view y_buf,
                       std::string_view rand_buf) {
  auto y_matrix = heu_matrix::CMatrix::LoadFrom(y_buf);
  T rand_num;
  rand_num.Deserialize(rand_buf);

  if constexpr (std::is_same_v<T, heu_phe::Ciphertext>) {
    heu_matrix::CMatrix rand_matrix(y_matrix.rows(), 1);
    for (int i = 0; i < y_matrix.rows(); ++i) {
      rand_matrix(i, 0) = rand_num;
    }
    return evaluator->Add(y_matrix, rand_matrix).Serialize();
  } else {
    heu_matrix::PMatrix rand_matrix(y_matrix.rows(), 1);
    for (int i = 0; i < y_matrix.rows(); ++i) {
      rand_matrix(i, 0) = rand_num;
    }
    return evaluator->Add(y_matrix, rand_matrix).Serialize();
  }
}

}  // namespace

PheReduce::PheReduce(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  // feature name
  partial_y_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "partial_y_col_name");
  rand_number_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "rand_number_col_name");
  select_peer_crypted_ =
      GetNodeAttr<bool>(opts_.node_def, "select_crypted_for_peer");

  BuildInputSchema();
  BuildOutputSchema();
}

void PheReduce::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 2,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->other_party_ids.size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE_EQ(ctx->requester_id, ctx->self_id);

  auto remote_party_id = *(ctx->other_party_ids.begin());

  std::shared_ptr<arrow::RecordBatch> peer_record_batch;
  std::shared_ptr<arrow::RecordBatch> self_record_batch;
  for (const auto& r : ctx->inputs.front()) {
    auto p_array = r->column(2);
    auto p = std::static_pointer_cast<arrow::StringArray>(p_array)->Value(0);
    if (p == remote_party_id) {
      peer_record_batch = r;
    } else {
      self_record_batch = r;
    }
  }
  SERVING_ENFORCE(peer_record_batch, errors::ErrorCode::UNEXPECTED_ERROR);
  SERVING_ENFORCE(self_record_batch, errors::ErrorCode::UNEXPECTED_ERROR);

  auto build_array = [](const ::yacl::Buffer& buf,
                        std::shared_ptr<arrow::Array>* array) {
    arrow::BinaryBuilder builder;
    SERVING_CHECK_ARROW_STATUS(builder.Append(buf.data<uint8_t>(), buf.size()));
    SERVING_CHECK_ARROW_STATUS(builder.Finish(array));
  };

  std::shared_ptr<arrow::Array> array;
  if (select_peer_crypted_) {
    const auto& evaluator =
        ctx->he_kit_mgm->GetDstMatrixEvaluator(remote_party_id);
    auto y_buf = std::static_pointer_cast<arrow::BinaryArray>(
                     self_record_batch->column(0))
                     ->Value(0);
    auto rand_buf = std::static_pointer_cast<arrow::BinaryArray>(
                        peer_record_batch->column(1))
                        ->Value(0);
    auto ye_matrix_buf =
        AddRand<heu_phe::Ciphertext>(evaluator.get(), y_buf, rand_buf);
    build_array(ye_matrix_buf, &array);
  } else {
    const auto& evaluator = ctx->he_kit_mgm->GetLocalMatrixEvaluator();
    auto y_buf = std::static_pointer_cast<arrow::BinaryArray>(
                     peer_record_batch->column(0))
                     ->Value(0);
    auto rand_buf = std::static_pointer_cast<arrow::BinaryArray>(
                        self_record_batch->column(1))
                        ->Value(0);
    auto ye_matrix_buf =
        AddRand<heu_phe::Plaintext>(evaluator.get(), y_buf, rand_buf);
    build_array(ye_matrix_buf, &array);
  }

  ctx->output = MakeRecordBatch(output_schema_, 1, {array});
}

void PheReduce::BuildInputSchema() {
  // build input schema
  input_schema_list_.emplace_back(
      arrow::schema({arrow::field(partial_y_col_name_, arrow::binary()),
                     arrow::field(rand_number_col_name_, arrow::binary()),
                     arrow::field("party", arrow::utf8())}));
}

void PheReduce::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(partial_y_col_name_, arrow::binary())});
}

REGISTER_OP_KERNEL(PHE_2P_REDUCE, PheReduce)
REGISTER_OP(PHE_2P_REDUCE, "0.0.1",
            "Two-party computation operator. Select data encrypted by either "
            "our side or the peer party "
            "according to the configuration.")
    .Mergeable()
    .StringAttr("partial_y_col_name",
                "The name of the partial_y column in the input and output",
                false, false)
    .StringAttr("rand_number_col_name",
                "The name of the rand number column in the input and output",
                false, false)
    .BoolAttr("select_crypted_for_peer",
              "If `True`, select the data can be decrypted by peer, "
              "including self calculated partial_y and peer's rand, "
              "otherwise select selfs.",
              false, false)
    .Input("compute results", "The compute results from both self and peer's")
    .Output("selected results", "The selected data");

}  // namespace secretflow::serving::op::phe_2p
