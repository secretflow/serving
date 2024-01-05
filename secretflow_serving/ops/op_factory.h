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

#include "secretflow_serving/core/singleton.h"
#include "secretflow_serving/ops/op_def_builder.h"

namespace secretflow::serving::op {

struct OpRegister {
  constexpr OpRegister operator<<(OpRegister o) const { return *this; }

  template <typename T>
  constexpr OpRegister operator<<(T&& v) const {
    return std::forward<T>(v)();
  }
};

class OpFactory final : public Singleton<OpFactory> {
 public:
  void Register(const std::shared_ptr<OpDef>& op_def) {
    std::lock_guard<std::mutex> lock(mutex_);
    SERVING_ENFORCE(op_defs_.emplace(op_def->name(), op_def).second,
                    errors::ErrorCode::LOGIC_ERROR,
                    "duplicated op_def registered for {}", op_def->name());
  }

  const std::shared_ptr<OpDef> Get(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = op_defs_.find(name);
    SERVING_ENFORCE(iter != op_defs_.end(), errors::ErrorCode::UNEXPECTED_ERROR,
                    "no op_def registered for {}", name);
    return iter->second;
  }

  std::vector<std::shared_ptr<const OpDef>> GetAllOps() {
    std::vector<std::shared_ptr<const OpDef>> result;

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : op_defs_) {
      result.emplace_back(pair.second);
    }
    return result;
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<OpDef>> op_defs_;
  std::mutex mutex_;
};

namespace internal {

class OpDefBuilderWrapper {
 public:
  explicit OpDefBuilderWrapper(std::string op_name, std::string version,
                               std::string desc)
      : builder_(std::move(op_name), std::move(version), std::move(desc)) {}

  OpDefBuilderWrapper& Int32Attr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<int32_t>> default_value = std::nullopt) {
    builder_.Int32Attr(std::move(name), std::move(desc), is_list, is_optional,
                       std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& Int64Attr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<int64_t>> default_value = std::nullopt) {
    builder_.Int64Attr(std::move(name), std::move(desc), is_list, is_optional,
                       std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& FloatAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<float>> default_value = std::nullopt) {
    builder_.FloatAttr(std::move(name), std::move(desc), is_list, is_optional,
                       std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& DoubleAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<double>> default_value = std::nullopt) {
    builder_.DoubleAttr(std::move(name), std::move(desc), is_list, is_optional,
                        std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& BoolAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<bool>> default_value = std::nullopt) {
    builder_.BoolAttr(std::move(name), std::move(desc), is_list, is_optional,
                      std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& StringAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<std::string>> default_value = std::nullopt) {
    builder_.StringAttr(std::move(name), std::move(desc), is_list, is_optional,
                        std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& BytesAttr(
      std::string name, std::string desc, bool is_list, bool is_optional,
      std::optional<AttrValueType<std::string>> default_value = std::nullopt) {
    builder_.BytesAttr(std::move(name), std::move(desc), is_list, is_optional,
                       std::move(default_value));
    return *this;
  }
  OpDefBuilderWrapper& Returnable() {
    builder_.Returnable();
    return *this;
  }
  OpDefBuilderWrapper& Mergeable() {
    builder_.Mergeable();
    return *this;
  }
  OpDefBuilderWrapper& Input(std::string name, std::string desc) {
    builder_.Input(std::move(name), std::move(desc));
    return *this;
  }
  OpDefBuilderWrapper& InputList(const std::string& prefix, size_t num,
                                 std::string desc) {
    for (size_t i = 0; i != num; ++i) {
      builder_.Input(prefix + std::to_string(i), desc);
    }
    return *this;
  }
  OpDefBuilderWrapper& Output(std::string name, std::string desc) {
    builder_.Output(std::move(name), std::move(desc));
    return *this;
  }

  OpRegister operator()() {
    OpFactory::GetInstance()->Register(builder_.Build());
    return {};
  }

 private:
  OpDefBuilder builder_;
};

}  // namespace internal

#define REGISTER_OP(op_name, version, desc)     \
  static OpRegister const regist_op_##op_name = \
      OpRegister{} << internal::OpDefBuilderWrapper(#op_name, version, desc)

}  // namespace secretflow::serving::op
