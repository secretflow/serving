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

#include "secretflow_serving/ops/he/test_utils.h"

namespace secretflow::serving::test {

Double::Matrix GenRawMatrix(int rows, int cols, int64_t start) {
  Double::Matrix m(rows, cols);
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      m(i, j) = start++;
    }
  }
  return m;
}

heu_matrix::PMatrix EncodeMatrix(const Double::Matrix& m,
                                 heu_phe::PlainEncoder* encoder) {
  int64_t ndim = 2;
  if (m.cols() == 1) {
    ndim = 1;
  }
  heu_matrix::PMatrix plain_matrix(m.rows(), m.cols(), ndim);
  for (int i = 0; i < plain_matrix.rows(); ++i) {
    for (int j = 0; j < plain_matrix.cols(); ++j) {
      plain_matrix(i, j) = encoder->Encode(m(i, j));
    }
  }

  return plain_matrix;
}

}  // namespace secretflow::serving::test
