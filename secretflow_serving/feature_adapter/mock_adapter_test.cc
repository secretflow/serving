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

#include "gtest/gtest.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"

namespace secretflow::serving::feature {
namespace {

const size_t kTestParamNum = 5;
const std::string kTestModelServiceId = "test_service_id";
const std::string kTestPartyId = "alice";
const std::string kTestContext = "test_context";

}  // namespace

class MockAdapterTest : public ::testing::Test {};

TEST_F(MockAdapterTest, Work) {
  FeatureSourceConfig config;
  (void)config.mutable_mock_opts();

  auto model_schema = arrow::schema(
      {arrow::field("x1", arrow::int32()), arrow::field("x2", arrow::int64()),
       arrow::field("x3", arrow::float32()),
       arrow::field("x4", arrow::float64()), arrow::field("x5", arrow::utf8()),
       arrow::field("x6", arrow::boolean())});

  auto adapter = FeatureAdapterFactory::GetInstance()->Create(
      config, kTestModelServiceId, kTestPartyId, model_schema);

  FeatureParam fs_param;
  for (size_t i = 0; i < kTestParamNum; ++i) {
    fs_param.add_query_datas(std::to_string(i));
  }
  fs_param.set_query_context(kTestContext);
  FeatureAdapter::Request request;
  request.fs_param = &fs_param;

  FeatureAdapter::Response response;

  ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
  ASSERT_TRUE(response.features);

  // check schema and rows
  auto schema = response.features->schema();
  EXPECT_TRUE(schema->Equals(model_schema));
  EXPECT_EQ(kTestParamNum, response.features->num_rows());
}

}  // namespace secretflow::serving::feature
