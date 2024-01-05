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

#include "benchmark/benchmark.h"

#include "secretflow_serving/core/link_func.h"
#include "secretflow_serving/ops/merge_y.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

static constexpr const char* const kLinkFuncsArray[] = {
    "LF_LOG",          "LF_LOGIT",       "LF_INVERSE",     "LF_RECIPROCAL",
    "LF_IDENTITY",     "LF_SIGMOID_RAW", "LF_SIGMOID_MM1", "LF_SIGMOID_MM3",
    "LF_SIGMOID_GA",   "LF_SIGMOID_T1",  "LF_SIGMOID_T3",  "LF_SIGMOID_T5",
    "LF_SIGMOID_T7",   "LF_SIGMOID_T9",  "LF_SIGMOID_LS7", "LF_SIGMOID_SEG3",
    "LF_SIGMOID_SEG5", "LF_SIGMOID_DF",  "LF_SIGMOID_SR",  "LF_SIGMOID_SEGLS"};

void BMMergeYOPBench(benchmark::State& state) {
  std::string json_content = R"JSON(
{
  "name": "test_node",
  "op": "MERGE_Y",
  "attr_values": {
    "input_col_name": {
      "s": "y"
    },
    "output_col_name": {
      "s": "score"
    }
  }
}
)JSON";

  auto link_func_index = state.range(2);
  auto party_nums = state.range(1);
  auto row_nums = state.range(0);

  state.counters["log2(row_nums*party_nums)"] = log2(row_nums * party_nums);
  state.counters["row_nums"] = row_nums;
  state.counters["party_nums"] = party_nums;
  state.counters["link_func_index"] = link_func_index;
  state.counters["log2(row_nums*party_nums)"] = log2(row_nums * party_nums);

  state.SetLabel(kLinkFuncsArray[link_func_index]);

  NodeDef node_def;
  JsonToPb(json_content, &node_def);
  {
    AttrValue link_func_value;
    link_func_value.set_s(kLinkFuncsArray[link_func_index]);
    node_def.mutable_attr_values()->insert(
        {"link_function", std::move(link_func_value)});
  }
  {
    AttrValue scale_value;
    scale_value.set_d(1.14);
    node_def.mutable_attr_values()->insert(
        {"yhat_scale", std::move(scale_value)});
  }

  // build node
  auto mock_node = std::make_shared<Node>(std::move(node_def));

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));

  // check input schema
  const auto& input_schema_list = kernel->GetAllInputSchema();

  // compute
  ComputeContext compute_ctx;
  std::vector<std::shared_ptr<arrow::RecordBatch>> input_list;

  // mock input values
  std::vector<std::vector<double>> feature_value_list;
  std::generate_n(std::back_inserter(feature_value_list), party_nums,
                  [row_nums]() {
                    std::vector<double> tmp;
                    std::generate_n(std::back_inserter(tmp), row_nums,
                                    [] { return rand(); });
                    return tmp;
                  });
  for (size_t i = 0; i < feature_value_list.size(); ++i) {
    arrow::DoubleBuilder builder;
    SERVING_CHECK_ARROW_STATUS(builder.AppendValues(feature_value_list[i]));
    std::shared_ptr<arrow::Array> array;
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
    input_list.emplace_back(
        MakeRecordBatch(input_schema_list.front(), row_nums, {array}));
  }
  compute_ctx.inputs.emplace_back(std::move(input_list));

  for (auto _ : state) {
    kernel->Compute(&compute_ctx);
  }
}

BENCHMARK(BMMergeYOPBench)
    ->ArgsProduct({
        benchmark::CreateRange(8, 1u << 25, /*multi=*/2),
        {1, 2, 3},
        benchmark::CreateDenseRange(
            0, 1,  // sizeof(kLinkFuncsArray) / sizeof(kLinkFuncsArray[0]) - 1,
            /*step=*/1),
    });

}  // namespace secretflow::serving::op
