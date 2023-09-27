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

#include <memory>
#include <string>
#include <vector>

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/protos/graph.pb.h"

namespace secretflow::serving::op {

#define DECLARE_GET_ATTR(TYPE)                                            \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   TYPE* value);                                          \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   std::vector<TYPE>* value);

DECLARE_GET_ATTR(std::string)
DECLARE_GET_ATTR(int32_t)
DECLARE_GET_ATTR(int64_t)
DECLARE_GET_ATTR(double)
DECLARE_GET_ATTR(float)
DECLARE_GET_ATTR(bool)
#undef DECLARE_GET_ATTR

template <typename T>
T GetNodeAttr(const NodeDef& node_def, const std::string& attr_name) {
  T value;
  if (!GetNodeAttr(node_def, attr_name, &value)) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "can not get attr:{} from node:{}, op:{}", attr_name,
                  node_def.name(), node_def.op());
  }
  return value;
}

}  // namespace secretflow::serving::op
