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

#include "secretflow_serving/ops/dot_product.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

template <typename EleT>
struct ArrowBuilderTrait {
  using type = void;
  constexpr static const char* const data_type = nullptr;
};

template <>
struct ArrowBuilderTrait<double> {
  using type = arrow::DoubleBuilder;
  constexpr static const char* const data_type = "DT_DOUBLE";
};

template <>
struct ArrowBuilderTrait<int64_t> {
  using type = arrow::Int64Builder;
  constexpr static const char* const data_type = "DT_INT64";
};

template <typename T>
void BMDotProductBench(benchmark::State& state) {
  auto feature_nums = state.range(1);
  auto row_nums = state.range(0);

  state.counters["log2(row_nums*feature_nums)"] = log2(row_nums * feature_nums);
  state.counters["feature_nums"] = feature_nums;
  state.counters["row_nums"] = row_nums;
  state.SetLabel(ArrowBuilderTrait<T>::data_type);

  // mock attr
  AttrValue feature_name_value;
  {
    std::vector<std::string> names;
    for (int64_t i = 1; i <= feature_nums; ++i) {
      names.push_back(fmt::format("x{}", i));
    }
    feature_name_value.mutable_ss()->mutable_data()->Assign(names.begin(),
                                                            names.end());
  }

  AttrValue input_types;
  {
    std::vector<std::string> types(feature_nums,
                                   ArrowBuilderTrait<T>::data_type);
    input_types.mutable_ss()->mutable_data()->Assign(types.begin(),
                                                     types.end());
  }

  AttrValue feature_weight_value;
  {
    std::vector<double> weights;
    std::generate_n(std::back_inserter(weights), feature_nums,
                    []() { return rand(); });

    feature_weight_value.mutable_ds()->mutable_data()->Assign(weights.begin(),
                                                              weights.end());
  }
  AttrValue output_col_name_value;
  output_col_name_value.set_s("score");
  AttrValue intercept_value;
  intercept_value.set_d(1.313201881559211);

  // mock feature values
  std::vector<std::vector<T>> feature_value_list;
  std::generate_n(std::back_inserter(feature_value_list), feature_nums,
                  [row_nums]() {
                    std::vector<T> tmp;
                    std::generate_n(std::back_inserter(tmp), row_nums,
                                    [] { return rand(); });
                    return tmp;
                  });

  NodeDef node_def;
  node_def.set_op_version("0.0.2");
  node_def.set_name("test_node");
  node_def.set_op("DOT_PRODUCT");
  node_def.mutable_attr_values()->insert({"feature_names", feature_name_value});
  node_def.mutable_attr_values()->insert(
      {"feature_weights", feature_weight_value});
  node_def.mutable_attr_values()->insert({"input_types", input_types});
  node_def.mutable_attr_values()->insert(
      {"output_col_name", output_col_name_value});
  node_def.mutable_attr_values()->insert({"intercept", intercept_value});
  auto mock_node = std::make_shared<Node>(std::move(node_def));

  OpKernelOptions opts{mock_node->node_def(), mock_node->GetOpDef()};
  auto kernel = OpKernelFactory::GetInstance()->Create(std::move(opts));
  const auto& input_schema_list = kernel->GetAllInputSchema();

  // compute
  ComputeContext compute_ctx;
  {
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    for (size_t i = 0; i < feature_value_list.size(); ++i) {
      typename ArrowBuilderTrait<T>::type builder;
      // arrow::DoubleBuilder builder;
      SERVING_CHECK_ARROW_STATUS(builder.AppendValues(feature_value_list[i]));
      std::shared_ptr<arrow::Array> array;
      SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
      arrays.emplace_back(array);
    }
    auto features =
        MakeRecordBatch(input_schema_list.front(), row_nums, std::move(arrays));
    compute_ctx.inputs.emplace_back(
        std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
  }
  for (auto _ : state) {
    kernel->Compute(&compute_ctx);
  }
}

BENCHMARK_TEMPLATE(BMDotProductBench, double)
    ->ArgsProduct({
        benchmark::CreateRange(2, 1u << 17, /*multi=*/2),
        benchmark::CreateRange(64, 512, /*multi=*/4),
    });

BENCHMARK_TEMPLATE(BMDotProductBench, int64_t)
    ->ArgsProduct({
        benchmark::CreateRange(2, 1u << 17, /*multi=*/2),
        benchmark::CreateRange(64, 512, /*multi=*/4),
    });

}  // namespace secretflow::serving::op
