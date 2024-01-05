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
                   : MockDataType::MDT_RANDOM;
}

void MockAdapter::OnFetchFeature(const Request& request, Response* response) {
  SERVING_ENFORCE(!request.fs_param->query_datas().empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "get empty feature service query datas.");
  size_t rows = request.fs_param->query_datas_size();
  size_t cols = feature_schema_->num_fields();
  std::vector<std::shared_ptr<arrow::Array>> arrays;

  for (size_t c = 0; c < cols; ++c) {
    std::shared_ptr<arrow::Array> array;
    const auto& f = feature_schema_->field(c);
    if (f->type()->id() == arrow::Type::type::BOOL) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : std::rand() % 2;
      };
      array = CreateArray<arrow::BooleanBuilder, bool>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::INT8) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : (std::rand() % std::numeric_limits<int8_t>::max());
      };
      array = CreateArray<arrow::Int8Builder, int8_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::UINT8) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : (std::numeric_limits<uint8_t>::max());
      };
      array = CreateArray<arrow::UInt8Builder, uint8_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::INT16) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : (std::numeric_limits<int16_t>::max());
      };
      array = CreateArray<arrow::Int16Builder, int16_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::UINT16) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : (std::numeric_limits<uint16_t>::max());
      };
      array = CreateArray<arrow::UInt16Builder, uint16_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::INT32) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : std::rand();
      };
      array = CreateArray<arrow::Int32Builder, int32_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::UINT32) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : std::rand();
      };
      array = CreateArray<arrow::UInt32Builder, uint32_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::INT64) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : std::rand();
      };
      array = CreateArray<arrow::Int64Builder, int64_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::UINT64) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED ? 1 : std::rand();
      };
      array = CreateArray<arrow::UInt64Builder, uint64_t>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::FLOAT) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : ((float)std::rand() / float(RAND_MAX));
      };
      array = CreateArray<arrow::FloatBuilder, float>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::DOUBLE) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? 1
                   : ((double)std::rand() / (RAND_MAX));
      };
      array = CreateArray<arrow::DoubleBuilder, double>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::STRING) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? "1"
                   : std::to_string(std::rand());
      };
      array = CreateArray<arrow::StringBuilder, std::string>(rows, generator);
    } else if (f->type()->id() == arrow::Type::type::BINARY) {
      const auto generator = [this] {
        return mock_type_ == MockDataType::MDT_FIXED
                   ? "1"
                   : std::to_string(std::rand());
      };
      array = CreateArray<arrow::BinaryBuilder, std::string>(rows, generator);
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
