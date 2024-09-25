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

#include "secretflow_serving/util/he_mgm.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving::he {

void HeKitMgm::InitLocalKit(yacl::ByteContainerView pk_buffer,
                            yacl::ByteContainerView sk_buffer,
                            int64_t encode_scale) {
  encode_scale_ = encode_scale;
  local_kit_ = std::make_unique<heu_phe::HeKit>(pk_buffer, sk_buffer);
  local_matrix_kit_ = std::make_unique<heu_matrix::HeKit>(*local_kit_);
  default_encoder_ = std::make_shared<heu_phe::PlainEncoder>(
      local_kit_->GetSchemaType(), encode_scale_);
}

void HeKitMgm::InitDstKit(const std::string& party,
                          yacl::ByteContainerView pk_buffer) {
  heu_phe::DestinationHeKit dst_kit(pk_buffer);
  heu_matrix::DestinationHeKit dst_matrix_kit(dst_kit);
  dst_kit_map_.emplace(
      party,
      std::make_pair<heu_phe::DestinationHeKit, heu_matrix::DestinationHeKit>(
          std::move(dst_kit), std::move(dst_matrix_kit)));
}

int64_t HeKitMgm::GetEncodeScale() {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return encode_scale_;
}

heu_phe::SchemaType HeKitMgm::GetSchemaType() {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return local_kit_->GetSchemaType();
}

std::shared_ptr<heu_phe::PlainEncoder> HeKitMgm::GetEncoder(int64_t scale) {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return std::make_shared<heu_phe::PlainEncoder>(local_kit_->GetSchemaType(),
                                                 scale);
}

std::shared_ptr<heu_phe::PlainEncoder> HeKitMgm::GetEncoder() {
  SERVING_ENFORCE(default_encoder_, errors::ErrorCode::LOGIC_ERROR);
  return default_encoder_;
}

const std::shared_ptr<heu_phe::Encryptor>& HeKitMgm::GetLocalEncryptor() {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return local_kit_->GetEncryptor();
}

const std::shared_ptr<heu_phe::Evaluator>& HeKitMgm::GetLocalEvaluator() {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return local_kit_->GetEvaluator();
}

const std::shared_ptr<heu_phe::Decryptor>& HeKitMgm::GetLocalDecryptor() {
  SERVING_ENFORCE(local_kit_, errors::ErrorCode::LOGIC_ERROR);

  return local_kit_->GetDecryptor();
}

const std::shared_ptr<heu_phe::Encryptor>& HeKitMgm::GetDstEncryptor(
    const std::string& party) {
  if (auto it = dst_kit_map_.find(party); it != dst_kit_map_.end()) {
    return it->second.first.GetEncryptor();
  }
  SERVING_THROW(errors::ErrorCode::LOGIC_ERROR,
                "can not find he kit for party: {}", party);
}

const std::shared_ptr<heu_phe::Evaluator>& HeKitMgm::GetDstEvaluator(
    const std::string& party) {
  if (auto it = dst_kit_map_.find(party); it != dst_kit_map_.end()) {
    return it->second.first.GetEvaluator();
  }
  SERVING_THROW(errors::ErrorCode::LOGIC_ERROR,
                "can not find he kit for party: {}", party);
}

const std::shared_ptr<heu_matrix::Encryptor>&
HeKitMgm::GetLocalMatrixEncryptor() {
  return local_matrix_kit_->GetEncryptor();
}

const std::shared_ptr<heu_matrix::Evaluator>&
HeKitMgm::GetLocalMatrixEvaluator() {
  return local_matrix_kit_->GetEvaluator();
}

const std::shared_ptr<heu_matrix::Decryptor>&
HeKitMgm::GetLocalMatrixDecryptor() {
  return local_matrix_kit_->GetDecryptor();
}

const std::shared_ptr<heu_matrix::Encryptor>& HeKitMgm::GetDstMatrixEncryptor(
    const std::string& party) {
  if (auto it = dst_kit_map_.find(party); it != dst_kit_map_.end()) {
    return it->second.second.GetEncryptor();
  }
  SERVING_THROW(errors::ErrorCode::LOGIC_ERROR,
                "can not find he kit for party: {}", party);
}

const std::shared_ptr<heu_matrix::Evaluator>& HeKitMgm::GetDstMatrixEvaluator(
    const std::string& party) {
  if (auto it = dst_kit_map_.find(party); it != dst_kit_map_.end()) {
    return it->second.second.GetEvaluator();
  }
  SERVING_THROW(errors::ErrorCode::LOGIC_ERROR,
                "can not find he kit for party: {}", party);
}

}  // namespace secretflow::serving::he
