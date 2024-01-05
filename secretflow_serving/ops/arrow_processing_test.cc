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

#include "secretflow_serving/ops/arrow_processing.h"

#include "arrow/compute/api.h"
#include "arrow/ipc/api.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/test_utils.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

namespace {
std::shared_ptr<arrow::RecordBatch> BuildRecordBatch(
    const std::vector<std::string>& array_jsons,
    std::vector<std::shared_ptr<arrow::Field>> fields) {
  std::vector<std::shared_ptr<arrow::Array>> input_arrays;
  std::shared_ptr<arrow::Array> tmp_array;
  for (size_t i = 0; i < array_jsons.size(); ++i) {
    SERVING_GET_ARROW_RESULT(arrow::ipc::internal::json::ArrayFromJSON(
                                 fields[i]->type(), array_jsons[i]),
                             tmp_array);
    input_arrays.emplace_back(tmp_array);
  }
  return MakeRecordBatch(arrow::schema(fields), tmp_array->length(),
                         std::move(input_arrays));
}
}  // namespace

struct Param {
  bool content_json_flag;
  std::vector<std::string> func_trace_contents;
  std::map<size_t, std::shared_ptr<arrow::compute::FunctionOptions>>
      func_trace_opts;

  std::vector<std::string> input_array_jsons;
  std::vector<std::shared_ptr<arrow::Field>> input_fields;

  std::vector<std::string> output_array_jsons;
  std::vector<std::shared_ptr<arrow::Field>> output_fields;
};

class ArrowProcessingParamTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(ArrowProcessingParamTest, Works) {
  auto param = GetParam();

  // build input & expect_output
  auto input = BuildRecordBatch(param.input_array_jsons, param.input_fields);

  auto expect_output =
      BuildRecordBatch(param.output_array_jsons, param.output_fields);
  std::cout << "expect_output: " << expect_output->ToString() << std::endl;

  // build node
  compute::ComputeTrace compute_trace;
  compute_trace.set_name("test_compute");
  for (size_t i = 0; i < param.func_trace_contents.size(); ++i) {
    auto* func_trace = compute_trace.add_func_traces();
    JsonToPb(param.func_trace_contents[i], func_trace);

    auto it = param.func_trace_opts.find(i);
    if (it != param.func_trace_opts.end()) {
      std::shared_ptr<arrow::Buffer> buf;
      SERVING_GET_ARROW_RESULT(it->second->Serialize(), buf);
      func_trace->set_option_bytes(reinterpret_cast<const char*>(buf->data()),
                                   buf->size());
    }
  }

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("ARROW_PROCESSING");

  AttrValue trace_content;
  AttrValue content_json_flag;
  if (param.content_json_flag) {
    trace_content.set_by(PbToJson(&compute_trace));
    content_json_flag.set_b(true);
    node_def.mutable_attr_values()->insert(
        {"content_json_flag", content_json_flag});
  } else {
    trace_content.set_by(compute_trace.SerializeAsString());
  }
  if (!param.func_trace_contents.empty()) {
    node_def.mutable_attr_values()->insert({"trace_content", trace_content});
  }

  {
    AttrValue input_schema_bytes;
    std::shared_ptr<arrow::Buffer> buf;
    SERVING_GET_ARROW_RESULT(arrow::ipc::SerializeSchema(*input->schema()),
                             buf);
    input_schema_bytes.set_by(reinterpret_cast<const char*>(buf->data()),
                              buf->size());
    node_def.mutable_attr_values()->insert(
        {"input_schema_bytes", std::move(input_schema_bytes)});
  }
  {
    AttrValue output_schema_bytes;
    std::shared_ptr<arrow::Buffer> buf;
    SERVING_GET_ARROW_RESULT(
        arrow::ipc::SerializeSchema(*expect_output->schema()), buf);
    output_schema_bytes.set_by(reinterpret_cast<const char*>(buf->data()),
                               buf->size());
    node_def.mutable_attr_values()->insert(
        {"output_schema_bytes", std::move(output_schema_bytes)});
  }

  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().returnable());

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  ASSERT_EQ(kernel->GetInputsNum(), mock_node->GetOpDef()->inputs_size());
  const auto& input_schema_list = kernel->GetAllInputSchema();
  ASSERT_EQ(input_schema_list.size(), kernel->GetInputsNum());
  for (const auto& input_schema : input_schema_list) {
    ASSERT_TRUE(input_schema->Equals(input->schema()));
  }

  // check output schema
  auto output_schema = kernel->GetOutputSchema();
  ASSERT_TRUE(output_schema->Equals(expect_output->schema()));

  // compute
  ComputeContext compute_ctx;
  compute_ctx.inputs.emplace_back(
      std::vector<std::shared_ptr<arrow::RecordBatch>>{input});

  kernel->Compute(&compute_ctx);

  // check output
  ASSERT_TRUE(compute_ctx.output);

  std::cout << "output: " << compute_ctx.output->ToString() << std::endl;

  double epsilon = 1E-13;
  ASSERT_TRUE(compute_ctx.output->ApproxEquals(
      *expect_output, arrow::EqualOptions::Defaults().atol(epsilon)));
}

INSTANTIATE_TEST_SUITE_P(
    ArrowProcessingParamTestSuite, ArrowProcessingParamTest,
    ::testing::Values(
        /*ext funcs*/
        Param{true,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
               R"JSON({
      "name": "add",
      "inputs": [
        {
          "data_id": 1
        },
        {
          "data_id": 2
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_SET_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "s2"
          }
        },
        {
          "data_id": 3
        }
      ],
      "output": {
        "data_id": 4
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 4
        },
        {
          "custom_scalar": {
            "i64": 2
          }
        }
      ],
      "output": {
        "data_id": 5
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_ADD_COLUMN",
      "inputs": [
        {
          "data_id": 5
        },
        {
          "custom_scalar": {
            "i64": 2
          }
        },
        {
          "custom_scalar": {
            "s": "a4"
          }
        },
        {
          "data_id": 3
        }
      ],
      "output": {
        "data_id": 6
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([5, 7, 9])JSON",
               R"JSON([5, 7, 9])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32()),
               arrow::field("a4", arrow::int32())}},
        /*run func with opts*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "round",
      "inputs": [
        {
          "data_id": 1
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_ADD_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "round_c"
          }
        },
        {
          "data_id": 2
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON"},
              {{1, std::make_shared<arrow::compute::RoundOptions>(
                       2, arrow::compute::RoundMode::DOWN)}},
              {R"JSON([1.234, 2.35864, 3.1415926])JSON"},
              {arrow::field("x1", arrow::float64())},
              {R"JSON([1.234, 2.35864, 3.1415926])JSON",
               R"JSON([1.23, 2.35, 3.14])JSON"},
              {arrow::field("x1", arrow::float64()),
               arrow::field("round_c", arrow::float64())}},
        /*run func with default opts*/
        Param{
            false,
            {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
             R"JSON({
      "name": "round",
      "inputs": [
        {
          "data_id": 1
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
             R"JSON({
      "name": "EFN_TB_ADD_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "round_c"
          }
        },
        {
          "data_id": 2
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON"},
            {},
            {R"JSON([1.234, 2.78864, 3.1415926])JSON"},
            {arrow::field("x1", arrow::float64())},
            {R"JSON([1.234, 2.78864, 3.1415926])JSON", R"JSON([1, 3, 3])JSON"},
            {arrow::field("x1", arrow::float64()),
             arrow::field("round_c", arrow::float64())}},
        /*dummy run*/
        Param{false,
              {},
              {},
              {R"JSON([1.234, 2.78864, 3.1415926])JSON"},
              {arrow::field("x1", arrow::float64())},
              {R"JSON([1.234, 2.78864, 3.1415926])JSON"},
              {arrow::field("x1", arrow::float64())}}));

class ArrowProcessingExceptionTest : public ::testing::TestWithParam<Param> {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_P(ArrowProcessingExceptionTest, Constructor) {
  auto param = GetParam();

  // build input & expect_output
  auto input = BuildRecordBatch(param.input_array_jsons, param.input_fields);

  auto expect_output =
      BuildRecordBatch(param.output_array_jsons, param.output_fields);

  // build node
  compute::ComputeTrace compute_trace;
  compute_trace.set_name("test_compute");
  for (size_t i = 0; i < param.func_trace_contents.size(); ++i) {
    auto* func_trace = compute_trace.add_func_traces();
    JsonToPb(param.func_trace_contents[i], func_trace);

    auto it = param.func_trace_opts.find(i);
    if (it != param.func_trace_opts.end()) {
      std::shared_ptr<arrow::Buffer> buf;
      SERVING_GET_ARROW_RESULT(it->second->Serialize(), buf);
      func_trace->set_option_bytes(reinterpret_cast<const char*>(buf->data()),
                                   buf->size());
    }
  }

  NodeDef node_def;
  node_def.set_name("test_node");
  node_def.set_op("ARROW_PROCESSING");

  // always use pb serialize
  AttrValue content_json_flag;
  content_json_flag.set_b(param.content_json_flag);
  node_def.mutable_attr_values()->insert(
      {"content_json_flag", content_json_flag});

  AttrValue trace_content;
  trace_content.set_by(compute_trace.SerializeAsString());
  node_def.mutable_attr_values()->insert({"trace_content", trace_content});

  {
    AttrValue input_schema_bytes;
    std::shared_ptr<arrow::Buffer> buf;
    SERVING_GET_ARROW_RESULT(arrow::ipc::SerializeSchema(*input->schema()),
                             buf);
    input_schema_bytes.set_by(reinterpret_cast<const char*>(buf->data()),
                              buf->size());
    node_def.mutable_attr_values()->insert(
        {"input_schema_bytes", std::move(input_schema_bytes)});
  }
  {
    AttrValue output_schema_bytes;
    std::shared_ptr<arrow::Buffer> buf;
    SERVING_GET_ARROW_RESULT(
        arrow::ipc::SerializeSchema(*expect_output->schema()), buf);
    output_schema_bytes.set_by(reinterpret_cast<const char*>(buf->data()),
                               buf->size());
    node_def.mutable_attr_values()->insert(
        {"output_schema_bytes", std::move(output_schema_bytes)});
  }

  // create kernel
  auto mock_node = std::make_shared<Node>(std::move(node_def));
  ASSERT_EQ(mock_node->GetOpDef()->inputs_size(), 1);
  ASSERT_TRUE(mock_node->GetOpDef()->tag().returnable());

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  try {
    OpKernelFactory::GetInstance()->Create(std::move(opts));
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
  }

  EXPECT_THROW(OpKernelFactory::GetInstance()->Create(std::move(opts)),
               Exception);
}

INSTANTIATE_TEST_SUITE_P(
    ArrowProcessingExceptionTest, ArrowProcessingExceptionTest,
    ::testing::Values(
        /*wrong content format*/ Param{true,
                                       {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
                                        R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
                                        R"JSON({
      "name": "add",
      "inputs": [
        {
          "data_id": 1
        },
        {
          "data_id": 2
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON",
                                        R"JSON({
      "name": "EFN_TB_SET_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "s2"
          }
        },
        {
          "data_id": 3
        }
      ],
      "output": {
        "data_id": 4
      }
    })JSON"},
                                       {},
                                       {R"JSON([1, 2, 3])JSON",
                                        R"JSON([4, 5, 6])JSON"},
                                       {arrow::field("x1", arrow::int32()),
                                        arrow::field("x2", arrow::int32())},
                                       {R"JSON([1, 2, 3])JSON",
                                        R"JSON([5, 7, 9])JSON"},
                                       {arrow::field("x1", arrow::int32()),
                                        arrow::field("s2", arrow::int32())}},
        /*invalid data_id*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 1
        },
        {
          "custom_scalar": {
            "i64": 2
          }
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*wrong data type*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 1
        },
        {
          "custom_scalar": {
            "i64": 2
          }
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*wrong index*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 5
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*wrong index*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": -2
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*wrong index type*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "data_id": 0
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON",
               R"JSON(["null", "null", "null"])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32()),
               arrow::field("x3", arrow::utf8())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*wrong input type*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
               R"JSON({
      "name": "add",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "data_id": 0
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_SET_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "s2"
          }
        },
        {
          "data_id": 3
        }
      ],
      "output": {
        "data_id": 4
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32())},
              {R"JSON([1, 2, 3])JSON", R"JSON([5, 7, 9])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*duplicate output_id*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "add",
      "inputs": [
        {
          "data_id": 1
        },
        {
          "data_id": 1
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_SET_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        },
        {
          "custom_scalar": {
            "s": "s2"
          }
        },
        {
          "data_id": 3
        }
      ],
      "output": {
        "data_id": 4
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32())},
              {R"JSON([1, 2, 3])JSON", R"JSON([5, 7, 9])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("s2", arrow::int32())}},
        /*not returnable*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32())},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32())}},
        /*require opts*/
        Param{false,
              {R"JSON({
      "name": "EFN_TB_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 0
          }
        }
      ],
      "output": {
        "data_id": 1
      }
    })JSON",
               R"JSON({
      "name": "is_in",
      "inputs": [
        {
          "data_id": 1
        }
      ],
      "output": {
        "data_id": 2
      }
    })JSON",
               R"JSON({
      "name": "EFN_TB_REMOVE_COLUMN",
      "inputs": [
        {
          "data_id": 0
        },
        {
          "custom_scalar": {
            "i64": 1
          }
        }
      ],
      "output": {
        "data_id": 3
      }
    })JSON"},
              {},
              {R"JSON([1, 2, 3])JSON", R"JSON([4, 5, 6])JSON"},
              {arrow::field("x1", arrow::int32()),
               arrow::field("x2", arrow::int32())},
              {R"JSON([1, 2, 3])JSON"},
              {arrow::field("x1", arrow::int32())}}));

}  // namespace secretflow::serving::op
