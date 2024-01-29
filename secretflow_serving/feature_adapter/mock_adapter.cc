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

#include "secretflow_serving/feature_adapter/mock_adapter.h"

#include <random>

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::feature {

namespace {

template <typename BuilderType, typename ValueType, typename Fn>
std::shared_ptr<arrow::Array> CreateArray(size_t rows, Fn generator) {
  BuilderType builder;
  std::vector<ValueType> values(rows);
  std::generate_n(values.begin(), rows, generator);
  SERVING_CHECK_ARROW_STATUS(builder.AppendValues(std::move(values)));
  std::shared_ptr<arrow::Array> array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
  return array;
}

}  // namespace

MockAdapter::MockAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : FeatureAdapter(spec, service_id, party_id, feature_schema) {
  SERVING_ENFORCE(spec_.has_mock_opts(), errors::ErrorCode::INVALID_ARGUMENT,
                  "invalid mock options");
  mock_type_ = spec_.mock_opts().type() != MockDataType::INVALID_MOCK_DATA_TYPE
                   ? spec_.mock_opts().type()
                   : MockDataType::MDT_FIXED;
}

void MockAdapter::OnFetchFeature(const Request& request, Response* response) {
  SERVING_ENFORCE(!request.fs_param->query_datas().empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "get empty feature service query datas.");
  size_t rows = request.fs_param->query_datas_size();
  size_t cols = feature_schema_->num_fields();
  std::vector<std::shared_ptr<arrow::Array>> arrays;

  std::mt19937 rand_gen;

  const auto int_generator = [&] {
    return mock_type_ == MockDataType::MDT_FIXED ? 1 : rand_gen() % 100;
  };
  const auto str_generator = [&] {
    return mock_type_ == MockDataType::MDT_FIXED
               ? "1"
               : std::to_string(rand_gen() % 100);
  };

  for (size_t c = 0; c < cols; ++c) {
    std::shared_ptr<arrow::Array> array;
    const auto& f = feature_schema_->field(c);
    if (f->type()->id() == arrow::Type::type::BOOL) {
      const auto generator = [&] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : rand_gen() % 2;
      };
      array = CreateArray<arrow::BooleanBuilder, bool>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::INT8) {
      array = CreateArray<arrow::Int8Builder, int8_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::UINT8) {
      array = CreateArray<arrow::UInt8Builder, uint8_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::INT16) {
      array = CreateArray<arrow::Int16Builder, int16_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::UINT16) {
      array = CreateArray<arrow::UInt16Builder, uint16_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::INT32) {
      array = CreateArray<arrow::Int32Builder, int32_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::UINT32) {
      array = CreateArray<arrow::UInt32Builder, uint32_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::INT64) {
      array = CreateArray<arrow::Int64Builder, int64_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::UINT64) {
      array = CreateArray<arrow::UInt64Builder, uint64_t>(rows, int_generator);
    } else if (f->type()->id() == arrow::Type::type::FLOAT) {
      const auto generator = [&] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : static_cast<float>(rand_gen() % 100) / 50;
      };
      array = CreateArray<arrow::FloatBuilder, float>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::DOUBLE) {
      const auto generator = [&] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : static_cast<double>(rand_gen() % 100) / 50;
      };
      array = CreateArray<arrow::DoubleBuilder, double>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::STRING) {
      array =
          CreateArray<arrow::StringBuilder, std::string>(rows, str_generator);
    } else if (f->type()->id() == arrow::Type::type::BINARY) {
      array =
          CreateArray<arrow::BinaryBuilder, std::string>(rows, str_generator);
    } else {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "unkown field type {}",
                    f->type()->ToString());
    }
    arrays.emplace_back(array);
  }
  response->features =
      MakeRecordBatch(feature_schema_, rows, std::move(arrays));
}

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kMockOpts, MockAdapter);

}  // namespace secretflow::serving::feature
