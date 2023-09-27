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

#include "Eigen/Core"

namespace secretflow::serving {

template <typename T>
struct TypeTrait {
  typedef T Scalar;
  typedef Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> Matrix;
  typedef Eigen::Matrix<T, Eigen::Dynamic, 1> ColVec;
  typedef Eigen::Matrix<T, 1, Eigen::Dynamic> RowVec;
};

typedef TypeTrait<float> Float;
typedef TypeTrait<double> Double;

enum class AlgorithmType {
  kRegression,
  kClassification,
};

}  // namespace secretflow::serving
