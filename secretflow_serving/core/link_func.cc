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

#include "secretflow_serving/core/link_func.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

namespace {

// refer to:
// https://github.com/secretflow/secretflow/blob/main/secretflow/utils/sigmoid.py#L22
template <typename T>
T T1Sig(T x, bool limit = true) {
  auto ret = 0.5F + 0.25F * x;
  if (limit) {
    ret = std::clamp<T>(ret, 0.0, 1.0);
  }
  return ret;
}

// refer to:
// https://github.com/secretflow/secretflow/blob/main/secretflow/utils/sigmoid.py#L36
template <typename T>
T T3Sig(T x, bool limit = true) {
  auto t3 = -1.0F / 48;
  auto ret = T1Sig(x, false) + std::pow(x, 3) * t3;
  if (limit) {
    if (x < -2) {
      return 0;
    } else if (x > 2) {
      return 1;
    }
  }
  return ret;
}

// refer to:
// https://github.com/secretflow/secretflow/blob/main/secretflow/utils/sigmoid.py#L81
template <typename T>
T SRSig(T x) {
  return 0.5F * (x / std::sqrt(1.0F + std::pow(x, 2))) + 0.5F;
}

}  // namespace

LinkFunctionType ParseLinkFuncType(const std::string& type) {
  LinkFunctionType lf_type;
  SERVING_ENFORCE(LinkFunctionType_Parse(type, &lf_type),
                  errors::ErrorCode::UNEXPECTED_ERROR,
                  "unsupported link func type:{}", type);
  return lf_type;
}

template <typename T>
T ApplyLinkFunc(T x, LinkFunctionType lf_type) {
  auto ls7 = [](T x) -> T {
    return 5.00052959e-01 + 2.35176260e-01 * x -
           3.97212202e-05 * std::pow(x, 2) - 1.23407424e-02 * std::pow(x, 3) +
           4.04588962e-06 * std::pow(x, 4) + 3.94330487e-04 * std::pow(x, 5) -
           9.74060972e-08 * std::pow(x, 6) - 4.74674505e-06 * std::pow(x, 7);
  };

  switch (lf_type) {
    case LinkFunctionType::LF_EXP: {
      return std::exp(x);
    }
    case LinkFunctionType::LF_RECIPROCAL: {
      return 1.0F / x;
    }
    case LinkFunctionType::LF_IDENTITY: {
      return x;
    }
    case LinkFunctionType::LF_SIGMOID_RAW: {
      return 1.0F / (1.0F + exp(-x));
    }
    case LinkFunctionType::LF_SIGMOID_MM1: {
      return 0.5F + 0.125F * x;
    }
    case LinkFunctionType::LF_SIGMOID_MM3: {
      return 0.5F + 0.197F * x - 0.004F * std::pow(x, 3);
    }
    case LinkFunctionType::LF_SIGMOID_GA: {
      return 0.5F + 0.15012F * x + 0.001593F * std::pow(x, 3);
    }
    case LinkFunctionType::LF_SIGMOID_T1: {
      return T1Sig(x);
    }
    case LinkFunctionType::LF_SIGMOID_T3: {
      return T3Sig(x);
    }
    case LinkFunctionType::LF_SIGMOID_T5: {
      // refer to
      // https://github.com/secretflow/secretflow/blob/main/secretflow/utils/sigmoid.py#L49
      auto t5 = 1.0F / 480;
      auto ret = T3Sig(x, false) + std::pow(x, 5) * t5;
      return std::clamp(ret, 0.0, 1.0);
    }
    case LinkFunctionType::LF_SIGMOID_T7: {
      return 0.5F + 0.25F * x - (1.0F / 48) * std::pow(x, 3) +
             (1.0F / 480) * std::pow(x, 5) - (17.0F / 80640) * std::pow(x, 7);
    }
    case LinkFunctionType::LF_SIGMOID_T9: {
      return 0.5F + 0.25F * x - (1.0F / 48) * std::pow(x, 3) +
             (1.0F / 480) * std::pow(x, 5) - (17.0F / 80640) * std::pow(x, 7) +
             (31.0F / 1451520) * std::pow(x, 9);
    }
    case LinkFunctionType::LF_SIGMOID_LS7: {
      return ls7(x);
    }
    case LinkFunctionType::LF_SIGMOID_SEG3: {
      if (x > 4) {
        return 1;
      } else if (x < -4) {
        return 0;
      } else {
        return 0.5F + 0.125F * x;
      }
    }
    case LinkFunctionType::LF_SIGMOID_SEG5: {
      if (x > 35.75) {
        return 1;
      } else if (x > 3.75) {
        return 0.965087890625 + x * 0.0009765625;
      } else if (x >= -3.75) {
        return 0.5 + 0.125 * x;
      } else if (x >= -35.75) {
        return 0.034912109375 + x * 0.0009765625;
      } else {
        return 0;
      }
    }
    case LinkFunctionType::LF_SIGMOID_DF: {
      // refer to:
      // https://github.com/secretflow/secretflow/blob/main/secretflow/utils/sigmoid.py#L71
      return 0.5F * (x / (1.0F + std::abs(x))) + 0.5F;
    }
    case LinkFunctionType::LF_SIGMOID_SR: {
      return SRSig(x);
    }
    case LinkFunctionType::LF_SIGMOID_SEGLS: {
      if (std::abs(x) <= 5.87) {
        return ls7(x);
      } else {
        return SRSig(x);
      }
    }
    default: {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "unsupported link func type {}", static_cast<int>(lf_type));
    }
  }
}

template float ApplyLinkFunc(float, LinkFunctionType);
template double ApplyLinkFunc(double, LinkFunctionType);

}  // namespace secretflow::serving
