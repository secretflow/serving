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

#include "secretflow_serving/ops/phe_linear/phe_2p_merge_y.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/he_mgm.h"

namespace secretflow::serving::op::phe_2p {

PheMergeY::PheMergeY(OpKernelOptions opts) : OpKernel(std::move(opts)) {
  decrypted_y_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "decrypted_y_col_name");
  crypted_y_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "crypted_y_col_name");
  score_col_name_ = GetNodeAttr<std::string>(opts_.node_def, "score_col_name");

  auto link_function_name =
      GetNodeAttr<std::string>(opts_.node_def, "link_function");
  link_function_ = ParseLinkFuncType(link_function_name);

  // optional attr
  yhat_scale_ =
      GetNodeAttr<double>(opts_.node_def, *opts_.op_def, "yhat_scale");
  exp_iters_ = GetNodeAttr<int32_t>(opts_.node_def, *opts_.op_def, "exp_iters");
  CheckLinkFuncAragsValid(link_function_, exp_iters_);

  BuildInputSchema();
  BuildOutputSchema();
}

void PheMergeY::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 2, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs[0].size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs[1].size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->other_party_ids.size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->he_kit_mgm, errors::ErrorCode::LOGIC_ERROR);

  const auto& decrypted_data = ctx->inputs[0].front();
  const auto& crypted_data = ctx->inputs[1].front();

  // add self rand number
  auto peer_ye_array = crypted_data->column(0);
  auto peer_ye_buf =
      std::static_pointer_cast<arrow::BinaryArray>(peer_ye_array)->Value(0);
  auto c_peer_ye_matrix = heu_matrix::CMatrix::LoadFrom(peer_ye_buf);

  // decrypt peer y
  auto matrix_decryptor = ctx->he_kit_mgm->GetLocalMatrixDecryptor();
  auto p_peer_ye_matrix = matrix_decryptor->Decrypt(c_peer_ye_matrix);

  // self y
  auto self_ye_array = decrypted_data->column(0);
  auto self_ye_buf =
      std::static_pointer_cast<arrow::BinaryArray>(self_ye_array)->Value(0);
  auto p_self_ye_matrix = heu_matrix::PMatrix::LoadFrom(self_ye_buf);

  // self_ye + peer_ye
  auto matrix_evaluator = ctx->he_kit_mgm->GetLocalMatrixEvaluator();
  auto p_score_matrix =
      matrix_evaluator->Add(p_self_ye_matrix, p_peer_ye_matrix);

  auto compute_encoder = ctx->he_kit_mgm->GetEncoder(
      he::kFeatureScale * ctx->he_kit_mgm->GetEncodeScale());

  std::shared_ptr<arrow::Array> score_array;
  arrow::DoubleBuilder score_builder;
  for (int i = 0; i < p_score_matrix.rows(); ++i) {
    auto score = compute_encoder->Decode<double>(p_score_matrix(i, 0));
    score = ApplyLinkFunc(score, link_function_, exp_iters_) * yhat_scale_;
    SERVING_CHECK_ARROW_STATUS(score_builder.Append(score));
  }
  SERVING_CHECK_ARROW_STATUS(score_builder.Finish(&score_array));

  ctx->output =
      MakeRecordBatch(output_schema_, score_array->length(), {score_array});
}

void PheMergeY::BuildInputSchema() {
  // build input schema
  input_schema_list_.emplace_back(
      arrow::schema({arrow::field(decrypted_y_col_name_, arrow::binary())}));
  input_schema_list_.emplace_back(
      arrow::schema({arrow::field(crypted_y_col_name_, arrow::binary())}));
}

void PheMergeY::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(score_col_name_, arrow::float64())});
}

REGISTER_OP_KERNEL(PHE_2P_MERGE_Y, PheMergeY)
REGISTER_OP(
    PHE_2P_MERGE_Y, "0.0.1",
    "Two-party computation operator. Merge the obfuscated partial_y decrypted "
    "by the peer party with the "
    "partial_y based on self own key to obtain the final prediction score.")
    .Returnable()
    .StringAttr("decrypted_y_col_name",
                "The name of the decrypted partial_y column in the first input",
                false, false)
    .StringAttr("crypted_y_col_name",
                "The name of the crypted partial_y column in the second input",
                false, false)
    .StringAttr("score_col_name", "The name of the score column in the output",
                false, false)
    .DoubleAttr(
        "yhat_scale",
        "In order to prevent value overflow, GLM training is performed on the "
        "scaled y label. So in the prediction process, you need to enlarge "
        "yhat back to get the real predicted value, `yhat = yhat_scale * "
        "link(X * W)`",
        false, true, 1.0)
    .StringAttr(
        "link_function",
        "Type of link function, defined in "
        "`secretflow_serving/protos/link_function.proto`. Optional value: "
        "LF_EXP, LF_EXP_TAYLOR, "
        "LF_RECIPROCAL, "
        "LF_IDENTITY, LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, "
        "LF_SIGMOID_GA, "
        "LF_SIGMOID_T1, LF_SIGMOID_T3, "
        "LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, "
        "LF_SIGMOID_SEG3, "
        "LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS",
        false, false)
    .Int32Attr("exp_iters",
               "Number of iterations of `exp` approximation, valid when "
               "`link_function` set `LF_EXP_TAYLOR`",
               false, true, 0)
    .Input("decrypted_data",
           "The decrypted data output by `PHE_2P_DECRYPT_PEER_Y`")
    .Input("crypted_data", "The crypted data selected by `PHE_2P_REDUCE`")
    .Output("score", "The final linear predict score.");

}  // namespace secretflow::serving::op::phe_2p
