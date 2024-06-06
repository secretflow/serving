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

#include "secretflow_serving/ops/graph.h"

#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

class MockOpKernel0 : public OpKernel {
 public:
  explicit MockOpKernel0(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_0", arrow::float64()),
                       arrow::field("test_field_1", arrow::float64()),
                       arrow::field("test_field_2", arrow::float64())});
    input_schema_list_ = {schema};
    output_schema_ = schema;
  }

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel1 : public OpKernel {
 public:
  explicit MockOpKernel1(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_1", arrow::float64()),
                       arrow::field("test_field_0", arrow::float64())});
    input_schema_list_ = {schema};
    output_schema_ =
        arrow::schema({arrow::field("test_field_a", arrow::float64())});
  }

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel2 : public OpKernel {
 public:
  explicit MockOpKernel2(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_a", arrow::float64())});
    input_schema_list_ = {schema};
    output_schema_ = schema;
  }

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel3 : public OpKernel {
 public:
  explicit MockOpKernel3(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema =
        arrow::schema({arrow::field("test_field_2", arrow::float64()),
                       arrow::field("test_field_0", arrow::float64())});
    auto schema1 =
        arrow::schema({arrow::field("test_field_a", arrow::float64())});
    input_schema_list_ = {schema, schema1};
    output_schema_ = schema1;
  }

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel4 : public OpKernel {
 public:
  explicit MockOpKernel4(OpKernelOptions opts) : OpKernel(std::move(opts)) {
    auto schema = arrow::schema({});
    input_schema_list_ = {schema};
    output_schema_ =
        arrow::schema({arrow::field("test_field_a", arrow::float64())});
  }

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

REGISTER_OP_KERNEL(TEST_OP_0, MockOpKernel0);
REGISTER_OP_KERNEL(TEST_OP_1, MockOpKernel1);
REGISTER_OP_KERNEL(TEST_OP_2, MockOpKernel2);
REGISTER_OP_KERNEL(TEST_OP_3, MockOpKernel3);
REGISTER_OP_KERNEL(TEST_OP_4, MockOpKernel4);
REGISTER_OP(TEST_OP_0, "0.0.1", "test_desc")
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_1, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_2, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Mergeable()
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_3, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Input("input2", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_4, "0.0.1", "test_desc")
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");

class GraphTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(GraphTest, Works) {
  std::string json_content = R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_0",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_c",
      "op": "TEST_OP_1",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_d",
      "op": "TEST_OP_3",
      "parents": [ "node_b", "node_c"  ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_e",
      "op": "TEST_OP_2",
      "parents": [ "node_d" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_f",
      "op": "TEST_OP_4",
      "parents": [ "node_e" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a", "node_b", "node_c", "node_d",
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_e", "node_f"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    },
  ]
}
)JSON";

  GraphDef graph_def;
  JsonToPb(json_content, &graph_def);

  Graph g(std::move(graph_def));
  const auto& execution_list = g.GetExecutions();
  EXPECT_EQ(execution_list.size(), 2);

  EXPECT_TRUE(execution_list[0]);
  EXPECT_EQ(execution_list[0]->id(), 0);
  EXPECT_TRUE(execution_list[0]->IsEntry());
  EXPECT_FALSE(execution_list[0]->IsExit());
  EXPECT_EQ(execution_list[0]->GetEntryNodeNum(), 1);
  EXPECT_EQ(execution_list[0]->GetExitNodeNum(), 1);
  EXPECT_FALSE(execution_list[0]->IsExitNode("node_b"));
  EXPECT_FALSE(execution_list[0]->IsExitNode("node_a"));
  EXPECT_FALSE(execution_list[0]->IsExitNode("node_c"));
  EXPECT_TRUE(execution_list[0]->IsExitNode("node_d"));

  EXPECT_TRUE(execution_list[1]);
  EXPECT_EQ(execution_list[1]->id(), 1);
  EXPECT_FALSE(execution_list[1]->IsEntry());
  EXPECT_TRUE(execution_list[1]->IsExit());
  EXPECT_EQ(execution_list[1]->GetEntryNodeNum(), 1);
  EXPECT_EQ(execution_list[1]->GetExitNodeNum(), 1);
  // EXPECT_TRUE(execution_list[1]->IsExitNode("node_e"));
  EXPECT_TRUE(execution_list[1]->IsExitNode("node_f"));
}

struct ErrorParam {
  std::string json_content;
};

class GraphErrorTest : public ::testing::TestWithParam<ErrorParam> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(GraphErrorTest, Works) {
  auto param = GetParam();

  GraphDef graph_def;
  JsonToPb(param.json_content, &graph_def);

  try {
    std::make_unique<Graph>(graph_def);
  } catch (const Exception& e) {
    std::cout << e.what() << std::endl;
  }

  ASSERT_THROW(std::make_unique<Graph>(std::move(graph_def)), Exception);
}

INSTANTIATE_TEST_SUITE_P(
    GraphErrorTestSuite, GraphErrorTest,
    ::testing::Values(/*multiple exit node*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_1",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_c",
      "op": "TEST_OP_1",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_b", "node_c"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    },
  ]
}
)JSON"},
                      /*duplicate node*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    }
  ]
}
)JSON"},
                      /*input node not found*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_1",
      "parents": [ "node_c" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_b"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    },
  ]
}
)JSON"},
                      /*exit node not returnable*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_0",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_b"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    },
  ]
}
)JSON"},
                      /*node not reachable*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_1",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    },
    {
      "name": "node_c",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a", "node_c"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_b"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    },
  ]
}
)JSON"},
                      /*unsupport execution num*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    }
  ]
}
)JSON"},
                      /*cycle*/ ErrorParam{R"JSON(
{
  "version": "0.0.1",
  "node_list": [
    {
      "name": "node_a",
      "op": "TEST_OP_0",
      "parents": [ "node_b" ],
      "attr_values": {
        "attr_s": {
          "s": "a"
        },
      },
    },
    {
      "name": "node_b",
      "op": "TEST_OP_1",
      "parents": [ "node_a" ],
      "attr_values": {
        "attr_s": {
          "s": "b"
        },
      },
    }
  ],
  "execution_list": [
    {
      "nodes": [
        "node_a"
      ],
      "config": {
        "dispatch_type": "DP_ALL"
      }
    },
    {
      "nodes": [
         "node_b"
      ],
      "config": {
        "dispatch_type": "DP_ANYONE"
      }
    }
  ]
}
)JSON"}));

// TODO: exception case

}  // namespace secretflow::serving::op
