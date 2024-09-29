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
  using Scalar = T;
  using Matrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;
  using ColVec = Eigen::Matrix<T, Eigen::Dynamic, 1>;
  using RowVec = Eigen::Matrix<T, 1, Eigen::Dynamic>;
};

using Float = TypeTrait<float>;
using Double = TypeTrait<double>;

}  // namespace secretflow::serving
