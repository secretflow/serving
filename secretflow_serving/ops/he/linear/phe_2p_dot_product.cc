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

#include "secretflow_serving/ops/he/linear/phe_2p_dot_product.h"

#include <set>

#include "yacl/crypto/rand/rand.h"

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/data_type.pb.h"

namespace secretflow::serving::op::phe_2p {

namespace {

heu_matrix::PMatrix TableToPMatrix(
    const std::shared_ptr<arrow::RecordBatch>& table,
    const heu_phe::PlainEncoder* encoder, const std::string& offset_col_name) {
  int rows = table->num_rows();
  int cols = table->num_columns();

  if (!offset_col_name.empty()) {
    SERVING_ENFORCE_EQ(table->column_name(cols - 1), offset_col_name);
    cols -= 1;
  }

  heu_matrix::PMatrix plain_matrix(rows, cols);
  for (int i = 0; i < cols; ++i) {
    auto double_array = CastToDoubleArray(table->column(i));
    for (int64_t j = 0; j < double_array->length(); ++j) {
      plain_matrix(j, i) = encoder->Encode(double_array->Value(j));
    }
  }

  return plain_matrix;
}

inline void BuildBinaryArray(const ::yacl::Buffer& buf,
                             std::shared_ptr<arrow::Array>* array) {
  arrow::BinaryBuilder builder;
  SERVING_CHECK_ARROW_STATUS(builder.Append(buf.data<uint8_t>(), buf.size()));
  SERVING_CHECK_ARROW_STATUS(builder.Finish(array));
}

}  // namespace

PheDotProduct::PheDotProduct(OpKernelOptions opts)
    : OpKernel(std::move(opts)), c_w_matrix_(1, 1) {
  // feature name
  feature_name_list_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node_def, *opts_.op_def, "feature_names");
  std::set<std::string> f_name_set;
  for (const auto& feature_name : feature_name_list_) {
    SERVING_ENFORCE(f_name_set.emplace(feature_name).second,
                    errors::ErrorCode::LOGIC_ERROR,
                    "found duplicate feature name:{}", feature_name);
  }
  // feature types
  feature_type_list_ = GetNodeAttr<std::vector<std::string>>(
      opts_.node_def, *opts_.op_def, "feature_types");
  SERVING_ENFORCE_EQ(feature_name_list_.size(), feature_type_list_.size(),
                     "attr:feature_names size={} does not match "
                     "attr:feature_types size={}, node:{}, op:{}",
                     feature_name_list_.size(), feature_type_list_.size(),
                     opts_.node_def.name(), opts_.node_def.op());
  // offset
  offset_col_name_ = GetNodeAttr<std::string>(opts_.node_def, *opts_.op_def,
                                              "offset_col_name");
  if (!offset_col_name_.empty()) {
    SERVING_ENFORCE(
        !feature_name_list_.empty(), errors::ErrorCode::LOGIC_ERROR,
        "attr:offset_col_name is set, but get empty attr:feature_names");
    SERVING_ENFORCE_EQ(feature_name_list_.back(), offset_col_name_,
                       "the offset column name must be placed at the end of "
                       "the feature list.");
  }

  result_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "result_col_name");
  rand_number_col_name_ =
      GetNodeAttr<std::string>(opts_.node_def, "rand_number_col_name");

  auto intercept_bytes = GetNodeBytesAttr<std::string>(
      opts_.node_def, *opts_.op_def, "intercept_ciphertext");
  if (!intercept_bytes.empty()) {
    has_intercept_ = true;
    try {
      c_intercept_.Deserialize(intercept_bytes);
    } catch (const std::exception& e) {
      SPDLOG_WARN(
          "failed to load intercept ciphertext, reason: {}. now try load "
          "intercept matrix "
          "ciphertext.",
          e.what());

      heu_matrix::CMatrix c_intercept_matrix(1, 1);
      try {
        c_intercept_matrix = heu_matrix::CMatrix::LoadFrom(intercept_bytes);
      } catch (const std::exception& ne) {
        SPDLOG_WARN("failed to load intercept ciphertext matrix, reason: {}",
                    ne.what());
        SERVING_THROW(
            errors::ErrorCode::UNEXPECTED_ERROR,
            "failed to load intercept for node({}), reason [{}] or [{}]",
            opts_.node_def.name(), e.what(), ne.what());
      }
      SERVING_ENFORCE_EQ(c_intercept_matrix.size(), 1);
      c_intercept_ = c_intercept_matrix(0, 0);
    }
  }

  if (feature_name_list_.empty()) {
    no_feature_ = true;
  } else if (feature_name_list_.size() == 1 && !offset_col_name_.empty()) {
    no_feature_ = true;
  } else {
    auto encrypted_weight_bytes = GetNodeBytesAttr<std::string>(
        opts_.node_def, "feature_weights_ciphertext");
    c_w_matrix_ = heu_matrix::CMatrix::LoadFrom(encrypted_weight_bytes);

    int32_t compute_feature_num = offset_col_name_.empty()
                                      ? feature_name_list_.size()
                                      : feature_name_list_.size() - 1;
    SERVING_ENFORCE_EQ(c_w_matrix_.ndim(), 1);
    SERVING_ENFORCE_EQ(c_w_matrix_.size(), compute_feature_num,
                       "The shape of weights ciphertext matrix mismatch with "
                       "the compute feature number, {} vs {}",
                       c_w_matrix_.size(), compute_feature_num);
  }

  BuildInputSchema();
  BuildOutputSchema();
}

void PheDotProduct::DoCompute(ComputeContext* ctx) {
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->other_party_ids.size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->he_kit_mgm, errors::ErrorCode::LOGIC_ERROR);

  auto remote_party_id = *(ctx->other_party_ids.begin());

  auto feature_encoder = ctx->he_kit_mgm->GetEncoder(he::kFeatureScale);
  auto compute_encoder = ctx->he_kit_mgm->GetEncoder(
      he::kFeatureScale * ctx->he_kit_mgm->GetEncodeScale());

  // 1. gen rand num E
  auto rand_num = yacl::crypto::FastRandU64();
  heu_phe::Plaintext p_e(ctx->he_kit_mgm->GetSchemaType(), rand_num);

  // 2.1. W * X + offset
  const auto& dst_m_evaluator =
      ctx->he_kit_mgm->GetDstMatrixEvaluator(remote_party_id);
  const heu_phe::Ciphertext zero_c =
      ctx->he_kit_mgm->GetDstEncryptor(remote_party_id)->EncryptZero();
  heu_matrix::CMatrix c_wx_matrix(ctx->inputs.front().front()->num_rows(), 1);
  if (no_feature_) {
    for (int32_t i = 0; i < c_wx_matrix.rows(); ++i) {
      c_wx_matrix(i, 0) = zero_c;
    }
  } else {
    auto p_x_matrix = TableToPMatrix(ctx->inputs.front().front(),
                                     feature_encoder.get(), offset_col_name_);
    c_wx_matrix = dst_m_evaluator->MatMul(p_x_matrix, c_w_matrix_);
  }
  // offset
  if (!offset_col_name_.empty()) {
    auto offset_array =
        ctx->inputs.front().front()->GetColumnByName(offset_col_name_);
    auto double_array = CastToDoubleArray(offset_array);
    heu_matrix::PMatrix p_offset_matrix(double_array->length(), 1);
    for (int64_t i = 0; i < double_array->length(); ++i) {
      p_offset_matrix(i, 0) = compute_encoder->Encode(double_array->Value(i));
    }
    c_wx_matrix = dst_m_evaluator->Add(c_wx_matrix, p_offset_matrix);
  }

  // 2.2 W * X + offset - E
  heu_matrix::PMatrix p_e_matrix(c_wx_matrix.shape());
  for (int i = 0; i < p_e_matrix.rows(); ++i) {
    for (int j = 0; j < p_e_matrix.cols(); ++j) {
      p_e_matrix(i, j) = p_e;
    }
  }
  auto c_wxe_matrix = dst_m_evaluator->Sub(c_wx_matrix, p_e_matrix);

  // 2.3 W * X + offset - E + I
  auto result_matrix = c_wxe_matrix;
  if (has_intercept_) {
    // intercept ciphertext * kFeatureScale
    const auto& evaluator = ctx->he_kit_mgm->GetDstEvaluator(remote_party_id);
    auto c_i = evaluator->Mul(c_intercept_, feature_encoder->Encode(1));

    heu_matrix::CMatrix c_i_matrix(c_wxe_matrix.shape());
    for (int i = 0; i < c_i_matrix.rows(); ++i) {
      for (int j = 0; j < c_i_matrix.cols(); ++j) {
        c_i_matrix(i, j) = c_i;
      }
    }
    result_matrix = dst_m_evaluator->Add(c_wxe_matrix, c_i_matrix);
  }

  // build result array
  auto result_buf = result_matrix.Serialize();
  std::shared_ptr<arrow::Array> wx_array;
  BuildBinaryArray(result_buf, &wx_array);

  // build rand num array
  std::shared_ptr<arrow::Array> e_array;
  if (ctx->requester_id == ctx->self_id) {
    // no need encrypt
    auto p_e_buf = p_e.Serialize();
    BuildBinaryArray(p_e_buf, &e_array);
  } else {
    auto c_e = ctx->he_kit_mgm->GetLocalEncryptor()->Encrypt(p_e);
    auto c_e_buf = c_e.Serialize();
    BuildBinaryArray(c_e_buf, &e_array);
  }

  // build party id array
  std::shared_ptr<arrow::Array> p_array;
  arrow::StringBuilder p_builder;
  SERVING_CHECK_ARROW_STATUS(p_builder.Append(ctx->self_id));
  SERVING_CHECK_ARROW_STATUS(p_builder.Finish(&p_array));

  ctx->output =
      MakeRecordBatch(output_schema_, 1, {wx_array, e_array, p_array});
}

void PheDotProduct::BuildInputSchema() {
  // build input schema
  std::vector<std::shared_ptr<arrow::Field>> fields;
  for (size_t i = 0; i < feature_name_list_.size(); ++i) {
    auto data_type = DataTypeToArrowDataType(feature_type_list_[i]);
    SERVING_ENFORCE(
        arrow::is_numeric(data_type->id()), errors::INVALID_ARGUMENT,
        "feature type must be numeric, get:{}", feature_type_list_[i]);
    fields.emplace_back(arrow::field(feature_name_list_[i], data_type));
  }

  if (!offset_col_name_.empty()) {
    SERVING_ENFORCE_EQ(
        fields.rbegin()->get()->name(), offset_col_name_,
        "offset column({}) must be the last column of the input schema.",
        offset_col_name_);
  }

  input_schema_list_.emplace_back(arrow::schema(std::move(fields)));
}

void PheDotProduct::BuildOutputSchema() {
  // build output schema
  output_schema_ =
      arrow::schema({arrow::field(result_col_name_, arrow::binary()),
                     arrow::field(rand_number_col_name_, arrow::binary()),
                     arrow::field("party", arrow::utf8())});
}

REGISTER_OP_KERNEL(PHE_2P_DOT_PRODUCT, PheDotProduct)
REGISTER_OP(
    PHE_2P_DOT_PRODUCT, "0.0.1",
    "Two-party computation operator. Load the encrypted feature weights, "
    "compute their dot product with the "
    "feature values, and add random noise to the result for obfuscation. Only "
    "supports computation between two parties, with the weights being "
    "encrypted using the other party's key.")
    .StringAttr("feature_names",
                "List of feature names. Note that if there is an offset "
                "column, it needs to be the last one in the list",
                true, true, std::vector<std::string>())
    .BytesAttr("feature_weights_ciphertext",
               "feature weight ciphertext matrix bytes", false, true,
               std::string())
    .StringAttr("feature_types",
                "List of input feature data types. Optional "
                "value: DT_UINT8, "
                "DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, "
                "DT_INT64, DT_FLOAT, DT_DOUBLE",
                true, true, std::vector<std::string>())
    .BytesAttr("intercept_ciphertext",
               "Intercept ciphertext bytes or matrix bytes", false, true,
               std::string())
    .StringAttr("offset_col_name",
                "The name of the offset column(feature) in the input", false,
                true, std::string())
    .StringAttr(
        "result_col_name",
        "The name of the calculation result(partial_y) column in the output",
        false, false)
    .StringAttr("rand_number_col_name",
                "The name of the generated rand number column in the output",
                false, false)
    .Input("features", "Input features")
    .Output("partial_y", "Calculation results");

}  // namespace secretflow::serving::op::phe_2p
