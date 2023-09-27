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

#include "secretflow_serving/protos/link_function.pb.h"

namespace secretflow::serving {

void ValidateLinkFuncType(const std::string& type) {
  LinkFucntionType lf_type;
  SERVING_ENFORCE(LinkFucntionType_Parse(type, &lf_type),
                  errors::ErrorCode::UNEXPECTED_ERROR,
                  "unsupport link func type:{}", type);
}

template <typename T>
T ApplyLinkFunc(T x, const std::string& type) {
  LinkFucntionType lf_type;
  SERVING_ENFORCE(LinkFucntionType_Parse(type, &lf_type),
                  errors::ErrorCode::UNEXPECTED_ERROR,
                  "unsupport link func type:{}", type);

  auto ls7 = [](T x) -> T {
    return 5.00052959e-01 + 2.35176260e-01 * x -
           3.97212202e-05 * std::pow(x, 2) - 1.23407424e-02 * std::pow(x, 3) +
           4.04588962e-06 * std::pow(x, 4) + 3.94330487e-04 * std::pow(x, 5) -
           9.74060972e-08 * std::pow(x, 6) - 4.74674505e-06 * std::pow(x, 7);
  };

  switch (lf_type) {
    case LinkFucntionType::LF_LOG: {
      return std::exp(x);
    }
    case LinkFucntionType::LF_LOGIT: {
      return 1.0f / (1.0f + std::exp(-x));
    }
    case LinkFucntionType::LF_INVERSE: {
      return std::exp(-x);
    }
    case LinkFucntionType::LF_LOGIT_V2: {
      return 0.5f * (x / std::sqrt(1 + std::pow(x, 2))) + 0.5f;
    }
    case LinkFucntionType::LF_RECIPROCAL: {
      return 1.0f / x;
    }
    case LinkFucntionType::LF_INDENTITY: {
      return x;
    }
    case LinkFucntionType::LF_SIGMOID_RAW: {
      return 1.0f / (1.0f + exp(-x));
    }
    case LinkFucntionType::LF_SIGMOID_MM1: {
      return 0.5f + 0.125f * x;
    }
    case LinkFucntionType::LF_SIGMOID_MM3: {
      return 0.5f + 0.197f * x - 0.004f * std::pow(x, 3);
    }
    case LinkFucntionType::LF_SIGMOID_GA: {
      return 0.5f + 0.15012f * x + 0.001593f * std::pow(x, 3);
    }
    case LinkFucntionType::LF_SIGMOID_T1: {
      return 0.5f + 0.25f * x;
    }
    case LinkFucntionType::LF_SIGMOID_T3: {
      return 0.5f + 0.25f * x - (1.0f / 48) * std::pow(x, 3);
    }
    case LinkFucntionType::LF_SIGMOID_T5: {
      return 0.5f + 0.25f * x - (1.0f / 48) * std::pow(x, 3) +
             (1.0f / 480) * std::pow(x, 5);
    }
    case LinkFucntionType::LF_SIGMOID_T7: {
      return 0.5f + 0.25f * x - (1.0f / 48) * std::pow(x, 3) +
             (1.0f / 480) * std::pow(x, 5) - (17.0f / 80640) * std::pow(x, 7);
    }
    case LinkFucntionType::LF_SIGMOID_T9: {
      return 0.5f + 0.25f * x - (1.0f / 48) * std::pow(x, 3) +
             (1.0f / 480) * std::pow(x, 5) - (17.0f / 80640) * std::pow(x, 7) +
             (31.0f / 1451520) * std::pow(x, 9);
    }
    case LinkFucntionType::LF_SIGMOID_LS7: {
      return ls7(x);
    }
    case LinkFucntionType::LF_SIGMOID_SEG3: {
      if (x > 4) {
        return 1;
      } else if (x < -4) {
        return 0;
      } else {
        return 0.5f + 0.125f * x;
      }
    }
    case LinkFucntionType::LF_SIGMOID_SEG5: {
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
    case LinkFucntionType::LF_SIGMOID_DF: {
      return 0.5f * (x / (1.f + std::abs(x))) + 0.5f;
    }
    case LinkFucntionType::LF_SIGMOID_SR: {
      return 0.5f * (x / std::sqrt(1.f + std::pow(x, 2))) + 0.5f;
    }
    case LinkFucntionType::LF_SIGMOID_SEGLS: {
      if (std::abs(x) <= 5.87) {
        return ls7(x);
      } else {
        return 0.5f * (x / std::sqrt(1.f + std::pow(x, 2))) + 0.5f;
      }
    }
    default: {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "not support link func type {}", static_cast<int>(lf_type));
    }
  }
}

template float ApplyLinkFunc(float, const std::string&);
template double ApplyLinkFunc(double, const std::string&);

}  // namespace secretflow::serving
