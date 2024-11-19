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

#include "secretflow_serving/ops/he/linear/phe_2p_decrypt_peer_y.h"

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/he_mgm.h"

namespace secretflow::serving::op::phe_2p {

PheDecryptPeerY::PheDecryptPeerY(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  // feature name
  partial_y_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "partial_y_col_name");
  decrypted_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "decrypted_col_name");

  BuildInputSchema();
  BuildOutputSchema();
}

void PheDecryptPeerY::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->other_party_ids.size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->he_kit_mgm, errors::ErrorCode::LOGIC_ERROR);

  // get peer y
  auto peer_y_array = ctx->inputs.front().front()->column(0);
  auto peer_y_buf =
      std::static_pointer_cast<arrow::BinaryArray>(peer_y_array)->Value(0);
  auto peer_y_matrix = heu_matrix::CMatrix::LoadFrom(peer_y_buf);

  // decrypt
  auto matrix_decryptor = ctx->he_kit_mgm->GetLocalMatrixDecryptor();
  auto p_y_matrix = matrix_decryptor->Decrypt(peer_y_matrix);
  auto p_y_buf = p_y_matrix.Serialize();

  std::shared_ptr<arrow::Array> ye_array;
  arrow::BinaryBuilder ye_builder;
  SERVING_CHECK_ARROW_STATUS(
      ye_builder.Append(p_y_buf.data<uint8_t>(), p_y_buf.size()));
  SERVING_CHECK_ARROW_STATUS(ye_builder.Finish(&ye_array));

  ctx->output = MakeRecordBatch(output_schema_, 1, {ye_array});
}

void PheDecryptPeerY::BuildInputSchema() {
  // build input schema
  input_schema_list_.emplace_back(
      arrow::schema({arrow::field(partial_y_col_name_, arrow::binary())}));
}

void PheDecryptPeerY::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(decrypted_col_name_, arrow::binary())});
}

REGISTER_OP_KERNEL(PHE_2P_DECRYPT_PEER_Y, PheDecryptPeerY)
REGISTER_OP(PHE_2P_DECRYPT_PEER_Y, "0.0.1",
            "Two-party computation operator. Decrypt the obfuscated partial_y "
            "and add a random number.")
    .StringAttr("partial_y_col_name",
                "The name of the partial_y(which can be decrypt by self) "
                "column in the input",
                false, false)
    .StringAttr("decrypted_col_name",
                "The name of the decrypted result column in the output", false,
                false)
    .Input("crypted_data", "Input feature table")
    .Output("decrypted_data",
            "Decrypted partial_y with the added random number.");

}  // namespace secretflow::serving::op::phe_2p
