// Copyright 2024 Ant Group Co., Ltd.
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

#include "secretflow_serving/feature_adapter/streaming_adapter.h"

#include "arrow/api.h"
#include "arrow/ipc/api.h"
#include "butil/files/temp_file.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/csv_util.h"

#include "secretflow_serving/protos/feature.pb.h"

DECLARE_bool(inferencer_mode);

namespace secretflow::serving::feature {

namespace {

const std::string kTestModelServiceId = "test_service_id";
const std::string kTestPartyId = "alice";

void CheckRecordBatchEqual(const std::shared_ptr<arrow::RecordBatch>& src,
                           const std::shared_ptr<arrow::RecordBatch>& dst) {
  SERVING_ENFORCE_EQ(src->num_columns(), dst->num_columns());
  CheckReferenceFields(src->schema(), dst->schema());

  for (int i = 0; i < dst->num_columns(); ++i) {
    auto dst_array = dst->column(i);
    auto src_array = src->GetColumnByName(dst->schema()->field_names()[i]);
    SERVING_ENFORCE(src_array, errors::ErrorCode::UNEXPECTED_ERROR);
    SERVING_ENFORCE(src_array->Equals(dst_array),
                    errors::ErrorCode::UNEXPECTED_ERROR);
  }
}

}  // namespace

class StreamingAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override { FLAGS_inferencer_mode = true; }
  void TearDown() override {}
};

TEST_F(StreamingAdapterTest, Works) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
)TEXT");

  FeatureSourceConfig config;
  auto* streaming_opts = config.mutable_streaming_opts();
  streaming_opts->set_file_path(tmpfile.fname());
  streaming_opts->set_id_name("id");

  std::vector<std::shared_ptr<arrow::Field>> fields = {
      arrow::field("x1", arrow::int32()), arrow::field("x2", arrow::float32()),
      arrow::field("x3", arrow::utf8())};
  auto model_schema = arrow::schema(fields);

  fields.emplace_back(arrow::field("id", arrow::utf8()));
  auto model_schema_with_id = arrow::schema(fields);

  auto adapter = FeatureAdapterFactory::GetInstance()->Create(
      config, kTestModelServiceId, kTestPartyId, model_schema);

  // fist request.
  FeatureParam fs_params;
  fs_params.add_query_datas("0");
  fs_params.set_query_context("test_contex_0");

  FeatureAdapter::Request request;
  request.fs_param = &fs_params;
  FeatureAdapter::Response response;

  ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
  ASSERT_TRUE(response.features);
  ASSERT_EQ(1, response.features->num_rows());
  CheckReferenceFields(response.features->schema(), model_schema);
  {
    std::shared_ptr<arrow::Array> x1;
    std::shared_ptr<arrow::Array> x2;
    std::shared_ptr<arrow::Array> x3;
    std::shared_ptr<arrow::Array> x4;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[0]"), x1);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[1.0]"), x2);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["alice"])"), x3);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["0"])"), x4);
    const std::shared_ptr<arrow::RecordBatch> expect_features =
        MakeRecordBatch(model_schema_with_id, 1, {x1, x2, x3, x4});
    CheckRecordBatchEqual(expect_features, response.features);
  }

  // second request
  fs_params.clear_query_datas();
  fs_params.add_query_datas("1");
  fs_params.set_query_context("test_contex_1");
  ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
  ASSERT_TRUE(response.features);
  ASSERT_EQ(1, response.features->num_rows());
  CheckReferenceFields(response.features->schema(), model_schema);
  {
    std::shared_ptr<arrow::Array> x1;
    std::shared_ptr<arrow::Array> x2;
    std::shared_ptr<arrow::Array> x3;
    std::shared_ptr<arrow::Array> x4;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[1]"), x1);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[11.0]"), x2);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["bob"])"), x3);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["1"])"), x4);
    const std::shared_ptr<arrow::RecordBatch> expect_features =
        MakeRecordBatch(model_schema_with_id, 1, {x1, x2, x3, x4});
    CheckRecordBatchEqual(expect_features, response.features);
  }

  // third request same as second request
  fs_params.clear_query_datas();
  fs_params.add_query_datas("1");
  fs_params.set_query_context("test_contex_1");
  ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
  ASSERT_TRUE(response.features);
  ASSERT_EQ(1, response.features->num_rows());
  CheckReferenceFields(response.features->schema(), model_schema);
  {
    std::shared_ptr<arrow::Array> x1;
    std::shared_ptr<arrow::Array> x2;
    std::shared_ptr<arrow::Array> x3;
    std::shared_ptr<arrow::Array> x4;
    using arrow::ipc::internal::json::ArrayFromJSON;
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::int32(), "[1]"), x1);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::float32(), "[11.0]"), x2);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["bob"])"), x3);
    SERVING_GET_ARROW_RESULT(ArrayFromJSON(arrow::utf8(), R"(["1"])"), x4);
    const std::shared_ptr<arrow::RecordBatch> expect_features =
        MakeRecordBatch(model_schema_with_id, 1, {x1, x2, x3, x4});
    CheckRecordBatchEqual(expect_features, response.features);
  }
}

struct StreamingModeParam {
  std::string content;
  int32_t adapter_block_size;
  int32_t request_block_size;
  std::shared_ptr<arrow::Schema> schema;
};

class StreamingAdapterStreamingModeTest
    : public ::testing::TestWithParam<StreamingModeParam> {
 protected:
  void SetUp() override { FLAGS_inferencer_mode = true; }
  void TearDown() override {}
};

TEST_P(StreamingAdapterStreamingModeTest, Works) {
  auto param = GetParam();

  butil::TempFile tmpfile;
  tmpfile.save(param.content.data());

  FeatureSourceConfig config;
  auto* streaming_opts = config.mutable_streaming_opts();
  streaming_opts->set_file_path(tmpfile.fname());
  streaming_opts->set_id_name("id");
  streaming_opts->set_block_size(param.adapter_block_size);

  auto adapter = FeatureAdapterFactory::GetInstance()->Create(
      config, kTestModelServiceId, kTestPartyId, param.schema);

  auto req_reader_opts = arrow::csv::ReadOptions::Defaults();
  if (param.request_block_size > 0) {
    req_reader_opts.block_size = param.request_block_size;
  }
  auto req_reader = csv::BuildStreamingReader(
      tmpfile.fname(), {{"id", arrow::utf8()}}, req_reader_opts);
  std::shared_ptr<arrow::RecordBatch> batch;
  while (true) {
    SERVING_CHECK_ARROW_STATUS(req_reader->ReadNext(&batch));
    if (batch == nullptr) {
      break;
    }
    std::cout << "req batch length: " << batch->num_rows() << '\n';

    auto id_array =
        std::static_pointer_cast<arrow::StringArray>(batch->column(0));

    FeatureParam fs_params;
    // fs_params.set_query_context(std::to_string(idx));
    for (int64_t i = 0; i < id_array->length(); ++i) {
      auto item = id_array->Value(i);
      fs_params.add_query_datas(item.data(), item.length());
    }
    FeatureAdapter::Request request;
    request.fs_param = &fs_params;
    FeatureAdapter::Response response;
    ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
    ASSERT_TRUE(response.features);
    ASSERT_EQ(batch->num_rows(), response.features->num_rows());
    CheckReferenceFields(response.features->schema(), param.schema);
  }
}

INSTANTIATE_TEST_SUITE_P(
    StreamingAdapterStreamingModeTestSuit, StreamingAdapterStreamingModeTest,
    ::testing::Values(
        StreamingModeParam{1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
)TEXT",
                           0, 0,
                           arrow::schema({arrow::field("x1", arrow::int32()),
                                          arrow::field("x2", arrow::float32()),
                                          arrow::field("x3", arrow::utf8())})},
        StreamingModeParam{1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
)TEXT",
                           15 /*read one row once*/, 0 /*request all*/,
                           arrow::schema({arrow::field("x1", arrow::int32()),
                                          arrow::field("x2", arrow::float32()),
                                          arrow::field("x3", arrow::utf8())})},
        StreamingModeParam{1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
3,3,31.0,dave,dummy
3,3,31.0,dave,dummy
)TEXT",
                           30 /*read two row once*/,
                           20 /*request one row once*/,
                           arrow::schema({arrow::field("x1", arrow::int32()),
                                          arrow::field("x2", arrow::float32()),
                                          arrow::field("x3", arrow::utf8())})},
        StreamingModeParam{
            1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
3,3,31.0,dave,dummy
3,3,31.0,dave,dummy
)TEXT",
            0 /*read all once*/, 20 /*request one row once*/,
            arrow::schema({arrow::field("x1", arrow::int32()),
                           arrow::field("x2", arrow::float32()),
                           arrow::field("x3", arrow::utf8())})}));

}  // namespace secretflow::serving::feature
