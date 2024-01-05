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

#include "secretflow_serving/ops/op_factory.h"

#include "gtest/gtest.h"

#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class OpFactoryTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(OpFactoryTest, Works) {
  // build expect attr
  std::map<std::string, std::string> attr_json_map = {{"attr_i32", R"JSON(
  {
    "name" : "attr_i32",
    "desc" : "attr_i32_desc",
    "type" : "AT_INT32",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_i64", R"JSON(
  {
    "name" : "attr_i64",
    "desc" : "attr_i64_desc",
    "type" : "AT_INT64",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_f", R"JSON(
  {
    "name" : "attr_f",
    "desc" : "attr_f_desc",
    "type" : "AT_FLOAT",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_d", R"JSON(
  {
    "name" : "attr_d",
    "desc" : "attr_d_desc",
    "type" : "AT_DOUBLE",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_b", R"JSON(
  {
    "name" : "attr_b",
    "desc" : "attr_b_desc",
    "type" : "AT_BOOL",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_s", R"JSON(
  {
    "name" : "attr_s",
    "desc" : "attr_s_desc",
    "type" : "AT_STRING",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_i32s", R"JSON(
  {
    "name" : "attr_i32s",
    "desc" : "attr_i32s_desc",
    "type" : "AT_INT32_LIST",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_i64s", R"JSON(
  {
    "name" : "attr_i64s",
    "desc" : "attr_i64s_desc",
    "type" : "AT_INT64_LIST",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_fs", R"JSON(
  {
    "name" : "attr_fs",
    "desc" : "attr_fs_desc",
    "type" : "AT_FLOAT_LIST",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_ds", R"JSON(
  {
    "name" : "attr_ds",
    "desc" : "attr_ds_desc",
    "type" : "AT_DOUBLE_LIST",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_bs", R"JSON(
  {
    "name" : "attr_bs",
    "desc" : "attr_bs_desc",
    "type" : "AT_BOOL_LIST",
    "is_optional" : "false",
  }
    )JSON"},
                                                      {"attr_ss", R"JSON(
  {
    "name" : "attr_ss",
    "desc" : "attr_ss_desc",
    "type" : "AT_STRING_LIST",
    "is_optional" : "false",
  }
    )JSON"}};

  REGISTER_OP(TEST_OP_0, "0.0.1", "test_desc")
      .Returnable()
      .Mergeable()
      .StringAttr("attr_ss", "attr_ss_desc", true, false)
      .BoolAttr("attr_bs", "attr_bs_desc", true, false)
      .DoubleAttr("attr_ds", "attr_ds_desc", true, false)
      .FloatAttr("attr_fs", "attr_fs_desc", true, false)
      .Int64Attr("attr_i64s", "attr_i64s_desc", true, false)
      .Int32Attr("attr_i32s", "attr_i32s_desc", true, false)
      .StringAttr("attr_s", "attr_s_desc", false, false)
      .BoolAttr("attr_b", "attr_b_desc", false, false)
      .DoubleAttr("attr_d", "attr_d_desc", false, false)
      .FloatAttr("attr_f", "attr_f_desc", false, false)
      .Int64Attr("attr_i64", "attr_i64_desc", false, false)
      .Int32Attr("attr_i32", "attr_i32_desc", false, false)
      .Input("input_1", "input_1_desc")
      .Input("input_0", "input_0_desc")
      .Output("output", "output_desc");
  // avoid unused warning
  regist_op_TEST_OP_0 << OpRegister{};

  auto op_def = OpFactory::GetInstance()->Get("TEST_OP_0");
  EXPECT_EQ(op_def->name(), "TEST_OP_0");
  EXPECT_EQ(op_def->version(), "0.0.1");
  EXPECT_EQ(op_def->desc(), "test_desc");

  // tag
  EXPECT_TRUE(op_def->tag().returnable());
  EXPECT_TRUE(op_def->tag().mergeable());

  // inputs & outputs
  for (int i = 0; i < op_def->inputs_size(); ++i) {
    const auto& input = op_def->inputs(i);
    std::cout << input.ShortDebugString() << std::endl;
    std::string name = "input_" + std::to_string(i);
    EXPECT_EQ(input.name(), name);
    EXPECT_EQ(input.desc(), name + "_desc");
  }
  EXPECT_EQ(op_def->output().name(), "output");
  EXPECT_EQ(op_def->output().desc(), "output_desc");

  // attr
  EXPECT_EQ(op_def->attrs_size(), attr_json_map.size());
  for (int i = 0; i < op_def->attrs_size(); ++i) {
    const auto& actual_attr_def = op_def->attrs(i);
    AttrDef expect_attr_def;
    JsonToPb(attr_json_map[actual_attr_def.name()], &expect_attr_def);
    EXPECT_FALSE(expect_attr_def.name().empty());
    EXPECT_FALSE(expect_attr_def.desc().empty());
    EXPECT_FALSE(expect_attr_def.type() == AttrType::UNKNOWN_AT_TYPE);

    EXPECT_EQ(expect_attr_def.name(), actual_attr_def.name());
    EXPECT_EQ(expect_attr_def.desc(), actual_attr_def.desc());
    EXPECT_EQ(expect_attr_def.type(), actual_attr_def.type());
    EXPECT_EQ(expect_attr_def.is_optional(), actual_attr_def.is_optional());
  }
}

TEST_F(OpFactoryTest, WorksDefaultValue) {
  // build expect attr
  std::map<std::string, std::string> attr_json_map = {{"attr_i32", R"JSON(
  {
    "name" : "attr_i32",
    "desc" : "attr_i32_desc",
    "type" : "AT_INT32",
    "is_optional" : "true",
    "default_value" : {
      "i32": 123
    }
  }
    )JSON"},
                                                      {"attr_i64", R"JSON(
  {
    "name" : "attr_i64",
    "desc" : "attr_i64_desc",
    "type" : "AT_INT64",
    "is_optional" : "true",
    "default_value" : {
      "i64": 1234
    }
  }
    )JSON"},
                                                      {"attr_f", R"JSON(
  {
    "name" : "attr_f",
    "desc" : "attr_f_desc",
    "type" : "AT_FLOAT",
    "is_optional" : "true",
    "default_value" : {
      "f": 0.12
    }
  }
    )JSON"},
                                                      {"attr_d", R"JSON(
  {
    "name" : "attr_d",
    "desc" : "attr_d_desc",
    "type" : "AT_DOUBLE",
    "is_optional" : "true",
    "default_value" : {
      "d": 0.13
    }
  }
    )JSON"},
                                                      {"attr_b", R"JSON(
  {
    "name" : "attr_b",
    "desc" : "attr_b_desc",
    "type" : "AT_BOOL",
    "is_optional" : "true",
    "default_value" : {
      "b": "true"
    }
  }
    )JSON"},
                                                      {"attr_s", R"JSON(
  {
    "name" : "attr_s",
    "desc" : "attr_s_desc",
    "type" : "AT_STRING",
    "is_optional" : "true",
    "default_value" : {
      "s": "s"
    }
  }
    )JSON"},
                                                      {"attr_i32s", R"JSON(
  {
    "name" : "attr_i32s",
    "desc" : "attr_i32s_desc",
    "type" : "AT_INT32_LIST",
    "is_optional" : "true",
    "default_value" : {
      "i32s": {
        "data": [1, 2]
      }
    }
  }
    )JSON"},
                                                      {"attr_i64s", R"JSON(
  {
    "name" : "attr_i64s",
    "desc" : "attr_i64s_desc",
    "type" : "AT_INT64_LIST",
    "is_optional" : "true",
    "default_value" : {
      "i64s": {
        "data": [3, 4]
      }
    }
  }
    )JSON"},
                                                      {"attr_fs", R"JSON(
  {
    "name" : "attr_fs",
    "desc" : "attr_fs_desc",
    "type" : "AT_FLOAT_LIST",
    "is_optional" : "true",
    "default_value" : {
      "fs": {
        "data": [1.1, 2.2]
      }
    }
  }
    )JSON"},
                                                      {"attr_ds", R"JSON(
  {
    "name" : "attr_ds",
    "desc" : "attr_ds_desc",
    "type" : "AT_DOUBLE_LIST",
    "is_optional" : "true",
    "default_value" : {
      "ds": {
        "data": [1.0, 2.0]
      }
    }
  }
    )JSON"},
                                                      {"attr_bs", R"JSON(
  {
    "name" : "attr_bs",
    "desc" : "attr_bs_desc",
    "type" : "AT_BOOL_LIST",
    "is_optional" : "true",
    "default_value" : {
      "bs": {
        "data": ["true", "false"]
      }
    }
  }
    )JSON"},
                                                      {"attr_ss", R"JSON(
  {
    "name" : "attr_ss",
    "desc" : "attr_ss_desc",
    "type" : "AT_STRING_LIST",
    "is_optional" : "true",
    "default_value" : {
      "ss": {
        "data": ["ss_0", "ss_1"]
      }
    }
  }
    )JSON"}};

  REGISTER_OP(TEST_OP_1, "0.0.1", "test_desc")
      .Returnable()
      .Mergeable()
      .StringAttr("attr_ss", "attr_ss_desc", true, true,
                  std::vector<std::string>{"ss_0", "ss_1"})
      .BoolAttr("attr_bs", "attr_bs_desc", true, true,
                std::vector<bool>{true, false})
      .DoubleAttr("attr_ds", "attr_ds_desc", true, true,
                  std::vector<double>{1.0d, 2.0d})
      .FloatAttr("attr_fs", "attr_fs_desc", true, true,
                 std::vector<float>{1.1f, 2.2f})
      .Int64Attr("attr_i64s", "attr_i64s_desc", true, true,
                 std::vector<int64_t>{3, 4})
      .Int32Attr("attr_i32s", "attr_i32s_desc", true, true,
                 std::vector<int32_t>{1, 2})
      .StringAttr("attr_s", "attr_s_desc", false, true, "s")
      .BoolAttr("attr_b", "attr_b_desc", false, true, true)
      .DoubleAttr("attr_d", "attr_d_desc", false, true, 0.13d)
      .FloatAttr("attr_f", "attr_f_desc", false, true, 0.12f)
      .Int64Attr("attr_i64", "attr_i64_desc", false, true, 1234)
      .Int32Attr("attr_i32", "attr_i32_desc", false, true, 123)
      .Input("input_1", "input_1_desc")
      .Input("input_0", "input_0_desc")
      .Output("output", "output_desc");
  // avoid unused warning
  regist_op_TEST_OP_1 << OpRegister{};

  auto op_def = OpFactory::GetInstance()->Get("TEST_OP_1");
  EXPECT_EQ(op_def->name(), "TEST_OP_1");
  EXPECT_EQ(op_def->version(), "0.0.1");
  EXPECT_EQ(op_def->desc(), "test_desc");

  // tag
  EXPECT_TRUE(op_def->tag().returnable());
  EXPECT_TRUE(op_def->tag().mergeable());

  // inputs & outputs
  for (int i = 0; i < op_def->inputs_size(); ++i) {
    const auto& input = op_def->inputs(i);
    std::cout << input.ShortDebugString() << std::endl;
    std::string name = "input_" + std::to_string(i);
    EXPECT_EQ(input.name(), name);
    EXPECT_EQ(input.desc(), name + "_desc");
  }
  EXPECT_EQ(op_def->output().name(), "output");
  EXPECT_EQ(op_def->output().desc(), "output_desc");

  // attr
  EXPECT_EQ(op_def->attrs_size(), attr_json_map.size());
  for (int i = 0; i < op_def->attrs_size(); ++i) {
    const auto& actual_attr_def = op_def->attrs(i);
    AttrDef expect_attr_def;
    JsonToPb(attr_json_map[actual_attr_def.name()], &expect_attr_def);
    EXPECT_FALSE(expect_attr_def.name().empty());
    EXPECT_FALSE(expect_attr_def.desc().empty());
    EXPECT_FALSE(expect_attr_def.type() == AttrType::UNKNOWN_AT_TYPE);

    std::cout << "expect " << expect_attr_def.ShortDebugString() << std::endl;

    std::cout << "actual " << actual_attr_def.ShortDebugString() << std::endl;

    std::cout << "-------------" << std::endl;

    EXPECT_EQ(expect_attr_def.name(), actual_attr_def.name());
    EXPECT_EQ(expect_attr_def.desc(), actual_attr_def.desc());
    EXPECT_EQ(expect_attr_def.type(), actual_attr_def.type());
    EXPECT_EQ(true, actual_attr_def.is_optional());

    auto default_value_equal_func = [](const AttrDef& expect,
                                       const AttrDef& actual) {
      return expect.default_value().i32() == actual.default_value().i32() &&
             expect.default_value().i64() == actual.default_value().i64() &&
             expect.default_value().f() == actual.default_value().f() &&
             expect.default_value().d() == actual.default_value().d() &&
             expect.default_value().s() == actual.default_value().s() &&
             expect.default_value().b() == actual.default_value().b() &&
             std::equal(actual.default_value().i32s().data().begin(),
                        actual.default_value().i32s().data().end(),
                        expect.default_value().i32s().data().begin()) &&
             std::equal(actual.default_value().i64s().data().begin(),
                        actual.default_value().i64s().data().end(),
                        expect.default_value().i64s().data().begin()) &&
             std::equal(actual.default_value().fs().data().begin(),
                        actual.default_value().fs().data().end(),
                        expect.default_value().fs().data().begin()) &&
             std::equal(actual.default_value().ds().data().begin(),
                        actual.default_value().ds().data().end(),
                        expect.default_value().ds().data().begin()) &&
             std::equal(actual.default_value().ss().data().begin(),
                        actual.default_value().ss().data().end(),
                        expect.default_value().ss().data().begin()) &&
             std::equal(actual.default_value().bs().data().begin(),
                        actual.default_value().bs().data().end(),
                        expect.default_value().bs().data().begin());
    };
    EXPECT_TRUE(default_value_equal_func(expect_attr_def, actual_attr_def));
  }
}

// TODO: default value case & duplicate case

}  // namespace secretflow::serving::op
