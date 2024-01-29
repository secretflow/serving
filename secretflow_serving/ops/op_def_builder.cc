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

#include "secretflow_serving/ops/op_def_builder.h"

#include "spdlog/spdlog.h"

namespace secretflow::serving::op {

OpDefBuilder::OpDefBuilder(std::string op_name, std::string version,
                           std::string desc)
    : name_(std::move(op_name)),
      version_(std::move(version)),
      desc_(std::move(desc)) {}

OpDefBuilder& OpDefBuilder::Int32Attr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<int32_t>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_INT32_LIST : AttrType::AT_INT32);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());

    if (!is_list) {
      auto& v = std::get<int32_t>(default_value.value());
      attr_def.mutable_default_value()->set_i32(v);
    } else {
      auto& v_list = std::get<std::vector<int32_t>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_i32s()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::Int64Attr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<int64_t>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_INT64_LIST : AttrType::AT_INT64);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<int64_t>(default_value.value());
      attr_def.mutable_default_value()->set_i64(v);
    } else {
      auto& v_list = std::get<std::vector<int64_t>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_i64s()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::FloatAttr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<float>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_FLOAT_LIST : AttrType::AT_FLOAT);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<float>(default_value.value());
      attr_def.mutable_default_value()->set_f(v);
    } else {
      auto& v_list = std::get<std::vector<float>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_fs()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::DoubleAttr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<double>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_DOUBLE_LIST : AttrType::AT_DOUBLE);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<double>(default_value.value());
      attr_def.mutable_default_value()->set_d(v);
    } else {
      auto& v_list = std::get<std::vector<double>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_ds()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::StringAttr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<std::string>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_STRING_LIST : AttrType::AT_STRING);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<std::string>(default_value.value());
      attr_def.mutable_default_value()->set_s(v);
    } else {
      auto& v_list = std::get<std::vector<std::string>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_ss()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::BoolAttr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<bool>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_BOOL_LIST : AttrType::AT_BOOL);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<bool>(default_value.value());
      attr_def.mutable_default_value()->set_b(v);
    } else {
      auto& v_list = std::get<std::vector<bool>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_bs()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::BytesAttr(
    std::string name, std::string desc, bool is_list, bool is_optional,
    std::optional<AttrValueType<std::string>> default_value) {
  AttrDef attr_def;
  attr_def.set_name(std::move(name));
  attr_def.set_desc(std::move(desc));
  attr_def.set_type(is_list ? AttrType::AT_BYTES_LIST : AttrType::AT_BYTES);
  attr_def.set_is_optional(is_optional);
  if (is_optional) {
    SERVING_ENFORCE(default_value.has_value(), errors::ErrorCode::LOGIC_ERROR,
                    "attr {}: default_value must be provided if optional",
                    attr_def.name());
    if (!is_list) {
      auto& v = std::get<std::string>(default_value.value());
      attr_def.mutable_default_value()->set_by(v);
    } else {
      auto& v_list = std::get<std::vector<std::string>>(default_value.value());
      *(attr_def.mutable_default_value()->mutable_bys()->mutable_data()) = {
          v_list.begin(), v_list.end()};
    }
  }

  SERVING_ENFORCE(
      attr_defs_.emplace(attr_def.name(), std::move(attr_def)).second,
      errors::ErrorCode::LOGIC_ERROR, "found duplicate attr:{}",
      attr_def.name());

  return *this;
}

OpDefBuilder& OpDefBuilder::Returnable() {
  returnable_ = true;
  return *this;
}

OpDefBuilder& OpDefBuilder::Mergeable() {
  mergeable_ = true;
  return *this;
}

OpDefBuilder& OpDefBuilder::VariableInputs() {
  variable_inputs_ = true;
  return *this;
}

OpDefBuilder& OpDefBuilder::Input(std::string name, std::string desc) {
  return Io(std::move(name), std::move(desc), false);
}

OpDefBuilder& OpDefBuilder::Output(std::string name, std::string desc) {
  return Io(std::move(name), std::move(desc), true);
}

OpDefBuilder& OpDefBuilder::Io(std::string name, std::string desc,
                               bool is_output) {
  if (is_output) {
    SERVING_ENFORCE(output_defs_.empty(), errors::ErrorCode::LOGIC_ERROR,
                    "should only have 1 output def.");
  }

  IoDef io_def;
  io_def.set_name(std::move(name));
  io_def.set_desc(std::move(desc));

  if (is_output) {
    output_defs_.emplace_back(std::move(io_def));
  } else {
    SERVING_ENFORCE(
        input_defs_.emplace(io_def.name(), std::move(io_def)).second,
        errors::ErrorCode::LOGIC_ERROR, "found duplicate input:{}",
        io_def.name());
  }

  return *this;
}

std::shared_ptr<OpDef> OpDefBuilder::Build() const {
  SERVING_ENFORCE(!output_defs_.empty(), errors::ErrorCode::LOGIC_ERROR,
                  "missing output def for op: {}", name_);

  if (variable_inputs_) {
    SERVING_ENFORCE_EQ(
        input_defs_.size(), 1U,
        "there should be only one input def for `variable inputs` op: {}",
        name_);
  }

  auto op_def = std::make_shared<OpDef>();
  op_def->set_name(name_);
  op_def->set_version(version_);
  op_def->set_desc(desc_);

  op_def->mutable_tag()->set_returnable(returnable_);
  op_def->mutable_tag()->set_mergeable(mergeable_);
  op_def->mutable_tag()->set_variable_inputs(variable_inputs_);

  // TODO: check valid
  for (const auto& pair : attr_defs_) {
    *(op_def->add_attrs()) = pair.second;
  }

  for (const auto& pair : input_defs_) {
    *(op_def->add_inputs()) = pair.second;
  }
  for (const auto& def : output_defs_) {
    *op_def->mutable_output() = def;
  }

  SPDLOG_DEBUG("op def: {}", op_def->ShortDebugString());
  return op_def;
}

}  // namespace secretflow::serving::op
