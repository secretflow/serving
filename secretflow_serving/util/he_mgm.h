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

#include "heu/library/numpy/numpy.h"
#include "heu/library/phe/encoding/plain_encoder.h"
#include "heu/library/phe/phe.h"

#include "secretflow_serving/core/singleton.h"

namespace heu_phe = ::heu::lib::phe;
namespace heu_matrix = ::heu::lib::numpy;

namespace secretflow::serving::he {

const int64_t kFeatureScale = 1e6;

class HeKitMgm : public Singleton<HeKitMgm> {
 public:
  void InitLocalKit(yacl::ByteContainerView pk_buffer,
                    yacl::ByteContainerView sk_buffer, int64_t encode_scale);
  void InitDstKit(const std::string& party, yacl::ByteContainerView pk_buffer);

  int64_t GetEncodeScale();

  heu_phe::SchemaType GetSchemaType();

  std::shared_ptr<heu_phe::PlainEncoder> GetEncoder(int64_t scale);

  std::shared_ptr<heu_phe::PlainEncoder> GetEncoder();

  // scalar
  [[nodiscard]] const std::shared_ptr<heu_phe::Encryptor>& GetLocalEncryptor();

  [[nodiscard]] const std::shared_ptr<heu_phe::Evaluator>& GetLocalEvaluator();

  [[nodiscard]] const std::shared_ptr<heu_phe::Decryptor>& GetLocalDecryptor();

  [[nodiscard]] const std::shared_ptr<heu_phe::Encryptor>& GetDstEncryptor(
      const std::string& party);

  [[nodiscard]] const std::shared_ptr<heu_phe::Evaluator>& GetDstEvaluator(
      const std::string& party);

  // matrix
  [[nodiscard]] const std::shared_ptr<heu_matrix::Encryptor>&
  GetLocalMatrixEncryptor();

  [[nodiscard]] const std::shared_ptr<heu_matrix::Evaluator>&
  GetLocalMatrixEvaluator();

  [[nodiscard]] const std::shared_ptr<heu_matrix::Decryptor>&
  GetLocalMatrixDecryptor();

  [[nodiscard]] const std::shared_ptr<heu_matrix::Encryptor>&
  GetDstMatrixEncryptor(const std::string& party);

  [[nodiscard]] const std::shared_ptr<heu_matrix::Evaluator>&
  GetDstMatrixEvaluator(const std::string& party);

 private:
  int64_t encode_scale_ = 0;

  std::unique_ptr<heu_phe::HeKit> local_kit_;
  std::unique_ptr<heu_matrix::HeKit> local_matrix_kit_;

  std::map<std::string,
           std::pair<heu_phe::DestinationHeKit, heu_matrix::DestinationHeKit>>
      dst_kit_map_;

  std::shared_ptr<heu_phe::PlainEncoder> default_encoder_;
};

}  // namespace secretflow::serving::he
