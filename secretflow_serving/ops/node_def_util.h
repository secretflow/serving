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

#include <string>
#include <vector>

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/protos/graph.pb.h"
#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving::op {

#define DECLARE_GET_ATTR(TYPE)                                            \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   TYPE* value);                                          \
  bool GetNodeAttr(const NodeDef& node_def, const std::string& attr_name, \
                   std::vector<TYPE>* value);                             \
  bool GetDefaultAttr(const OpDef& op_def, const std::string& attr_name,  \
                      TYPE* value);                                       \
  bool GetDefaultAttr(const OpDef& op_def, const std::string& attr_name,  \
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

template <typename T>
T GetNodeAttr(const NodeDef& node_def, const OpDef& op_def,
              const std::string& attr_name) {
  T value;
  if (!GetNodeAttr(node_def, attr_name, &value)) {
    if (!GetDefaultAttr(op_def, attr_name, &value)) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "can not get attr:{} from node:{}, op:{}", attr_name,
                    node_def.name(), node_def.op());
    }
  }
  return value;
}

bool GetNodeBytesAttr(const NodeDef& node_def, const std::string& attr_name,
                      std::string* value);
bool GetNodeBytesAttr(const NodeDef& node_def, const std::string& attr_name,
                      std::vector<std::string>* value);
bool GetBytesDefaultAttr(const OpDef& op_def, const std::string& attr_name,
                         std::string* value);
bool GetBytesDefaultAttr(const OpDef& op_def, const std::string& attr_name,
                         std::vector<std::string>* value);

template <typename T, typename Enable = void>
struct string_type_check : std::false_type {};

template <>
struct string_type_check<std::string> : std::true_type {};

template <>
struct string_type_check<std::vector<std::string>> : std::true_type {};

template <typename T, std::enable_if_t<string_type_check<T>::value, int> = 0>
T GetNodeBytesAttr(const NodeDef& node_def, const std::string& attr_name) {
  T value;
  if (!GetNodeBytesAttr(node_def, attr_name, &value)) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "can not get bytes attr:{} from node:{}, op:{}", attr_name,
                  node_def.name(), node_def.op());
  }
  return value;
}

template <typename T, std::enable_if_t<string_type_check<T>::value, int> = 0>
T GetNodeBytesAttr(const NodeDef& node_def, const OpDef& op_def,
                   const std::string& attr_name) {
  T value;
  if (!GetNodeBytesAttr(node_def, attr_name, &value)) {
    if (!GetBytesDefaultAttr(op_def, attr_name, &value)) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "can not get default attr:{} from op:{}", attr_name,
                    node_def.op());
    }
  }
  return value;
}

template <typename T>
void CheckAttrValueDuplicate(const std::vector<T>& items,
                             const std::string& attr_name) {
  std::set<T> item_set;
  for (const auto& item : items) {
    SERVING_ENFORCE(item_set.emplace(item).second,
                    errors::ErrorCode::LOGIC_ERROR,
                    "found duplicate item:{} in {}", item, attr_name);
  }
}

template <typename T>
void CheckAttrValueDuplicate(const std::vector<T>& items,
                             const std::string& attr_name, T ignore_item) {
  std::set<T> item_set;
  for (const auto& item : items) {
    if (item != ignore_item) {
      SERVING_ENFORCE(item_set.emplace(item).second,
                      errors::ErrorCode::LOGIC_ERROR,
                      "found duplicate item:{} in {}", item, attr_name);
    }
  }
}

}  // namespace secretflow::serving::op
