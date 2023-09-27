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

#include "secretflow_serving/ops/node_def_util.h"

namespace secretflow::serving::op {

namespace {

bool GetAttrValue(const NodeDef& node_def, const std::string& attr_name,
                  AttrValue* attr_value) {
  auto iter = node_def.attr_values().find(attr_name);
  if (iter != node_def.attr_values().end()) {
    *attr_value = iter->second;
    return true;
  }
  return false;
}

}  // namespace

#define DEFINE_GETT_LIST_ATTR(TYPE, FIELD_LIST, CAST)                     \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   std::vector<TYPE>* value) {                            \
    AttrValue attr_value;                                                 \
    if (!GetAttrValue(node_def, attr_name, &attr_value)) {                \
      return false;                                                       \
    }                                                                     \
    SERVING_ENFORCE(                                                      \
        attr_value.has_##FIELD_LIST(), errors::ErrorCode::LOGIC_ERROR,    \
        "attr_value({}) does not have expected type({}) value, node: {}", \
        attr_name, #FIELD_LIST, node_def.name());                         \
    SERVING_ENFORCE(!attr_value.FIELD_LIST().data().empty(),              \
                    errors::ErrorCode::INVALID_ARGUMENT,                  \
                    "attr_value({}) type({}) has empty value, node: {}",  \
                    attr_name, #FIELD_LIST, node_def.name());             \
    value->reserve(attr_value.FIELD_LIST().data().size());                \
    for (const auto& v : attr_value.FIELD_LIST().data()) {                \
      value->emplace_back(CAST);                                          \
    }                                                                     \
    return true;                                                          \
  }

#define DEFINE_GET_ATTR(TYPE, FIELD, CAST)                                \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   TYPE* value) {                                         \
    AttrValue attr_value;                                                 \
    if (!GetAttrValue(node_def, attr_name, &attr_value)) {                \
      return false;                                                       \
    }                                                                     \
    SERVING_ENFORCE(                                                      \
        attr_value.has_##FIELD(), errors::ErrorCode::LOGIC_ERROR,         \
        "attr_value({}) does not have expected type({}) value, node: {}", \
        attr_name, #FIELD, node_def.name());                              \
    const auto& v = attr_value.FIELD();                                   \
    *value = CAST;                                                        \
    return true;                                                          \
  }                                                                       \
  DEFINE_GETT_LIST_ATTR(TYPE, FIELD##s, CAST)

DEFINE_GET_ATTR(std::string, s, v)
DEFINE_GET_ATTR(int64_t, i64, v)
DEFINE_GET_ATTR(float, f, v)
DEFINE_GET_ATTR(int32_t, i32, v)
DEFINE_GET_ATTR(double, d, v)
DEFINE_GET_ATTR(bool, b, v)
#undef DEFINE_GET_ATTR

}  // namespace secretflow::serving::op
