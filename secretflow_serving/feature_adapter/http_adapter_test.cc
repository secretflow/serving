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

#include "brpc/server.h"
#include "fmt/format.h"
#include "google/protobuf/util/json_util.h"
#include "gtest/gtest.h"
#include "json2pb/rapidjson.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/spis/batch_feature_service.pb.h"
#include "secretflow_serving/spis/error_code.pb.h"

namespace secretflow::serving::feature {

namespace {
const size_t kTestParamNum = 3;
const std::string kTestHeaderKey = "test_header";
const std::string kTestHeaderValue = "test_value";
const std::string kTestModelServiceId = "test_service_id";
const std::string kTestPartyId = "alice";
const std::string kTestContext = "test_context";
const std::vector<std::string> kFieldNames = {"x1", "x2", "x3",
                                              "x4", "x5", "x6"};
const std::vector<FieldType> kFieldTypes = {
    FieldType::FIELD_INT32,  FieldType::FIELD_INT64,  FieldType::FIELD_FLOAT,
    FieldType::FIELD_DOUBLE, FieldType::FIELD_STRING, FieldType::FIELD_BOOL};
const std::vector<int32_t> kI32Values = {1, 2, 3};
const std::vector<int64_t> kI64Values = {4, 5, 6};
const std::vector<float> kFValues = {7.0F, 8.0F, 9.0F};
const std::vector<double> kDValues = {1.1, 2.2, 3.3};
const std::vector<std::string> kStrValues = {"a", "b", "c"};
const std::vector<bool> kBValues = {true, false, true};
}  // namespace

class MockFeatureService
    : public ::secretflow::serving::spis::BatchFeatureService {
 public:
  void BatchFetchFeature(::google::protobuf::RpcController *controller,
                         const spis::BatchFetchFeatureRequest *request,
                         spis::BatchFetchFeatureResponse *response,
                         ::google::protobuf::Closure *done) override {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller *cntl = static_cast<brpc::Controller *>(controller);
    cntl->set_always_print_primitive_fields(true);

    auto status = response->mutable_status();
    status->set_code(spis::ErrorCode::OK);

    // check header
    // check model_service_id
    // check party_id
    // check context
    if (request->header().data().at(kTestHeaderKey) != kTestHeaderValue ||
        request->model_service_id() != kTestModelServiceId ||
        request->party_id() != kTestPartyId ||
        request->feature_fields().empty() ||
        request->param().query_datas_size() != kTestParamNum ||
        request->param().query_context() != kTestContext) {
      status->set_code(spis::ErrorCode::INTERNAL_ERROR);
      return;
    }

    // check items equal
    const auto &fs_param = request->param();
    for (int i = 0; i < fs_param.query_datas_size(); ++i) {
      if (fs_param.query_datas(i) != std::to_string(i)) {
        status->set_code(spis::ErrorCode::INTERNAL_ERROR);
        return;
      }
    }

    // fill up response
    response->mutable_header()->mutable_data()->insert(
        {kTestHeaderKey, kTestHeaderValue});
    for (int c = 0; c < request->feature_fields_size(); ++c) {
      auto f = response->add_features();
      f->mutable_field()->CopyFrom(request->feature_fields(c));
      switch (f->field().type()) {
        case FieldType::FIELD_INT32: {
          f->mutable_value()->mutable_i32s()->Assign(kI32Values.begin(),
                                                     kI32Values.end());
          break;
        }
        case FieldType::FIELD_INT64: {
          f->mutable_value()->mutable_i64s()->Assign(kI64Values.begin(),
                                                     kI64Values.end());
          break;
        }
        case FieldType::FIELD_FLOAT: {
          f->mutable_value()->mutable_fs()->Assign(kFValues.begin(),
                                                   kFValues.end());
          break;
        }
        case FieldType::FIELD_DOUBLE: {
          f->mutable_value()->mutable_ds()->Assign(kDValues.begin(),
                                                   kDValues.end());
          break;
        }
        case FieldType::FIELD_STRING: {
          f->mutable_value()->mutable_ss()->Assign(kStrValues.begin(),
                                                   kStrValues.end());
          break;
        }
        case FieldType::FIELD_BOOL: {
          f->mutable_value()->mutable_bs()->Assign(kBValues.begin(),
                                                   kBValues.end());
          break;
        }
        default:
          break;
      }
    }
  }
};

class HttpAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override { StartBrpcServer(); }
  void TearDown() override {}
  void StartBrpcServer() {
    server_ = std::make_unique<brpc::Server>();
    {
      feature_service_ = new MockFeatureService();
      ASSERT_TRUE(server_->AddService(feature_service_,
                                      brpc::SERVER_OWNS_SERVICE) == 0);
    }
    // listen on IP_ANY : random port.
    ASSERT_TRUE(server_->Start(0, nullptr) == 0);
    listen_port_ = server_->listen_address().port;
  }

 protected:
  std::unique_ptr<brpc::Server> server_;
  MockFeatureService *feature_service_;
  int32_t listen_port_;
};

TEST_F(HttpAdapterTest, Work) {
  FeatureSourceConfig config;
  auto http_opts = config.mutable_http_opts();
  http_opts->set_endpoint(fmt::format(
      "http://0.0.0.0:{}/BatchFeatureService/BatchFetchFeature", listen_port_));

  auto model_schema = arrow::schema(
      {arrow::field("x1", arrow::int32()), arrow::field("x2", arrow::int64()),
       arrow::field("x3", arrow::float32()),
       arrow::field("x4", arrow::float64()), arrow::field("x5", arrow::utf8()),
       arrow::field("x6", arrow::boolean())});
  // build expect features
  std::shared_ptr<arrow::Array> array_1;
  std::shared_ptr<arrow::Array> array_2;
  std::shared_ptr<arrow::Array> array_3;
  std::shared_ptr<arrow::Array> array_4;
  std::shared_ptr<arrow::Array> array_5;
  std::shared_ptr<arrow::Array> array_6;
  arrow::Int32Builder i32_builder;
  SERVING_CHECK_ARROW_STATUS(i32_builder.AppendValues(kI32Values));
  SERVING_CHECK_ARROW_STATUS(i32_builder.Finish(&array_1));
  arrow::Int64Builder i64_builder;
  SERVING_CHECK_ARROW_STATUS(i64_builder.AppendValues(kI64Values));
  SERVING_CHECK_ARROW_STATUS(i64_builder.Finish(&array_2));
  arrow::FloatBuilder f_builder;
  SERVING_CHECK_ARROW_STATUS(f_builder.AppendValues(kFValues));
  SERVING_CHECK_ARROW_STATUS(f_builder.Finish(&array_3));
  arrow::DoubleBuilder d_builder;
  SERVING_CHECK_ARROW_STATUS(d_builder.AppendValues(kDValues));
  SERVING_CHECK_ARROW_STATUS(d_builder.Finish(&array_4));
  arrow::StringBuilder s_builder;
  SERVING_CHECK_ARROW_STATUS(s_builder.AppendValues(kStrValues));
  SERVING_CHECK_ARROW_STATUS(s_builder.Finish(&array_5));
  arrow::BooleanBuilder b_builder;
  SERVING_CHECK_ARROW_STATUS(b_builder.AppendValues(kBValues));
  SERVING_CHECK_ARROW_STATUS(b_builder.Finish(&array_6));
  auto expect_features =
      MakeRecordBatch(model_schema, kTestParamNum,
                      {array_1, array_2, array_3, array_4, array_5, array_6});

  auto adapter = FeatureAdapterFactory::GetInstance()->Create(
      config, kTestModelServiceId, kTestPartyId, model_schema);

  apis::Header req_header;
  req_header.mutable_data()->insert({kTestHeaderKey, kTestHeaderValue});
  FeatureParam fs_param;
  for (size_t i = 0; i < kTestParamNum; ++i) {
    fs_param.add_query_datas(std::to_string(i));
  }
  fs_param.set_query_context(kTestContext);
  FeatureAdapter::Request request;
  request.header = &req_header;
  request.fs_param = &fs_param;

  apis::Header res_header;
  FeatureAdapter::Response response;
  response.header = &res_header;

  ASSERT_NO_THROW(adapter->FetchFeature(request, &response));
  ASSERT_TRUE(response.features);
  ASSERT_TRUE(response.features->ApproxEquals(*expect_features));
}

}  // namespace secretflow::serving::feature
