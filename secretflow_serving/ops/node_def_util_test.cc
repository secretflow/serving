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

#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"

#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class NodeDefUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(NodeDefUtilTest, Works) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "test_op",
  "attr_values": {
    "attr_i32": {
      "i32": 1
    },
    "attr_i64": {
      "i64": 2
    },
    "attr_f": {
      "f": 3.0
    },
    "attr_d": {
      "d": 4.0
    },
    "attr_s": {
      "s": "5"
    },
    "attr_b": {
      "b": "true"
    },
    "attr_i32s": {
      "i32s": {
        "data": [ 1, 2, 3 ]
      }
    },
    "attr_i64s": {
      "i64s": {
        "data": [ 4, 5, 6 ]
      }
    },
    "attr_fs": {
      "fs": {
        "data": [ 7.0, 8.0, 9.0 ]
      }
    },
    "attr_ds": {
      "ds": {
        "data": [ 1.1, 2.2 ]
      }
    },
    "attr_ss": {
      "ss": {
        "data": [ "a", "b" ]
      }
    },
    "attr_bs": {
      "bs": {
        "data": [ "true", "false" ]
      }
    },
  }
}
)JSON";

  NodeDef node_def;
  JsonToPb(json_content, &node_def);

  {
    int32_t attr_i32;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_i32", &attr_i32));
    EXPECT_EQ(attr_i32, 1);
  }
  {
    int64_t attr_i64;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_i64", &attr_i64));
    EXPECT_EQ(attr_i64, 2);
  }
  {
    float attr_f;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_f", &attr_f));
    EXPECT_EQ(attr_f, 3.0f);
  }
  {
    double attr_d;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_d", &attr_d));
    EXPECT_EQ(attr_d, 4.0d);
  }
  {
    std::string attr_s;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_s", &attr_s));
    EXPECT_EQ(attr_s, "5");
  }
  {
    bool attr_b;
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_b", &attr_b));
    EXPECT_EQ(attr_b, true);
  }
  {
    std::vector<int32_t> attr_i32s;
    std::vector<int32_t> expect_i32s = {1, 2, 3};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_i32s", &attr_i32s));
    EXPECT_TRUE(
        std::equal(attr_i32s.begin(), attr_i32s.end(), expect_i32s.begin()));
  }
  {
    std::vector<int64_t> attr_i64s;
    std::vector<int64_t> expect_i64s = {4, 5, 6};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_i64s", &attr_i64s));
    EXPECT_TRUE(
        std::equal(attr_i64s.begin(), attr_i64s.end(), expect_i64s.begin()));
  }
  {
    std::vector<float> attr_fs;
    std::vector<float> expect_fs = {7.0, 8.0, 9.0};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_fs", &attr_fs));
    EXPECT_TRUE(std::equal(attr_fs.begin(), attr_fs.end(), expect_fs.begin()));
  }
  {
    std::vector<double> attr_ds;
    std::vector<double> expect_ds = {1.1, 2.2};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_ds", &attr_ds));
    EXPECT_TRUE(std::equal(attr_ds.begin(), attr_ds.end(), expect_ds.begin()));
  }
  {
    std::vector<std::string> attr_ss;
    std::vector<std::string> expect_ss = {"a", "b"};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_ss", &attr_ss));
    EXPECT_TRUE(std::equal(attr_ss.begin(), attr_ss.end(), expect_ss.begin()));
  }
  {
    std::vector<bool> attr_bs;
    std::vector<bool> expect_bs = {true, false};
    EXPECT_TRUE(GetNodeAttr(node_def, "attr_bs", &attr_bs));
    EXPECT_TRUE(std::equal(attr_bs.begin(), attr_bs.end(), expect_bs.begin()));
  }

  // not found case
  EXPECT_THROW(GetNodeAttr<bool>(node_def, "attr_b_test"), Exception);

  // type mismatch
  EXPECT_THROW(GetNodeAttr<std::vector<int32_t>>(node_def, "attr_ss"),
               Exception);
  EXPECT_THROW(GetNodeAttr<double>(node_def, "attr_ss"), Exception);
}

TEST_F(NodeDefUtilTest, OneOfError) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "test_op",
  "attr_values": {
    "attr_i32": {
      "i32": 1,
      "ss": {
        "data": [ "a", "b" ]
      }
    }
  }
}
)JSON";

  NodeDef node_def;
  //
  // msg:
  // invalid value oneof field 'value' is already set. Cannot set 'ss' for type
  // oneof
  //
  EXPECT_THROW(JsonToPb(json_content, &node_def), Exception);
}

}  // namespace secretflow::serving::op
