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

#include "secretflow_serving/feature_adapter/file_adapter.h"

#include "arrow/api.h"
#include "arrow/ipc/api.h"
#include "butil/files/temp_file.h"
#include "gtest/gtest.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/protos/feature.pb.h"

namespace secretflow::serving::feature {

namespace {

const std::string kTestModelServiceId = "test_service_id";
const std::string kTestPartyId = "alice";

}  // namespace

class FileAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(FileAdapterTest, Work) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
)TEXT");

  FeatureSourceConfig config;
  auto* csv_opts = config.mutable_csv_opts();
  csv_opts->set_file_path(tmpfile.fname());
  csv_opts->set_id_name("id");

  auto model_schema = arrow::schema({arrow::field("x1", arrow::int32()),
                                     arrow::field("x2", arrow::float32()),
                                     arrow::field("x3", arrow::utf8())});

  auto adapter = FeatureAdapterFactory::GetInstance()->Create(
      config, kTestModelServiceId, kTestPartyId, model_schema);
  {
    FeatureParam fs_params;
    fs_params.add_query_datas("0");
    fs_params.add_query_datas("1");
    // no query "2" to check data filter
    fs_params.add_query_datas("3");

    FeatureAdapter::Request request;
    request.fs_param = &fs_params;

    FeatureAdapter::Response response;

    ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
    ASSERT_TRUE(response.features);

    ASSERT_EQ(3, response.features->num_rows());
    ASSERT_EQ(3, response.features->num_columns());

    std::shared_ptr<arrow::Array> x1, x2, x3;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[0, 1, 3]"), x1);
    SERVING_GET_ARROW_RESULT(
        ArrayFromJSON(arrow::float32(), "[1.0, 11.0, 31.0]"), x2);
    SERVING_GET_ARROW_RESULT(
        ArrayFromJSON(arrow::utf8(), R"(["alice", "bob", "dave"])"), x3);

    const std::shared_ptr<arrow::RecordBatch> expect_features =
        MakeRecordBatch(model_schema, 3, {x1, x2, x3});
    ASSERT_TRUE(response.features->Equals(*expect_features));
  }
}

}  // namespace secretflow::serving::feature
