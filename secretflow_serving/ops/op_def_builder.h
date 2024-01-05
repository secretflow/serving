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

#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/protos/op.pb.h"

namespace secretflow::serving::op {

template <class S>
using AttrValueType = std::variant<S, std::vector<S>>;

class OpDefBuilder final {
 public:
  explicit OpDefBuilder(std::string op_name, std::string version,
                        std::string desc);

  // attr
  OpDefBuilder& Int32Attr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<int32_t>> default_value = std::nullopt);
  OpDefBuilder& Int64Attr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<int64_t>> default_value = std::nullopt);
  OpDefBuilder& FloatAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<float>> default_value = std::nullopt);
  OpDefBuilder& DoubleAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<double>> default_value = std::nullopt);
  OpDefBuilder& BoolAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<bool>> default_value = std::nullopt);
  OpDefBuilder& StringAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<std::string>> default_value = std::nullopt);
  OpDefBuilder& BytesAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<std::string>> default_value = std::nullopt);

  // tag
  OpDefBuilder& Returnable();
  OpDefBuilder& Mergeable();

  // io
  OpDefBuilder& Input(std::string name, std::string desc);
  OpDefBuilder& Output(std::string name, std::string desc);

  std::shared_ptr<OpDef> Build() const;

 protected:
  OpDefBuilder& Io(std::string name, std::string desc, bool is_output);

 private:
  const std::string name_;
  const std::string version_;
  const std::string desc_;

  bool returnable_ = false;
  bool mergeable_ = false;

  std::unordered_map<std::string, AttrDef> attr_defs_;
  std::unordered_map<std::string, IoDef> input_defs_;
  std::vector<IoDef> output_defs_;
};

}  // namespace secretflow::serving::op
