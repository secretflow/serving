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

#include "secretflow_serving/feature_adapter/http_adapter.h"

#include "google/protobuf/util/json_util.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/network.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/spis/batch_feature_service.pb.h"
#include "secretflow_serving/spis/error_code.pb.h"

namespace secretflow::serving::feature {

namespace {

const size_t kConnectTimeoutMs = 500;

const size_t kTimeoutMs = 1000;

errors::ErrorCode MappingErrorCode(int fs_code) {
  // spis::ErrorCode --> errors::ErrorCode
  const static std::map<int, errors::ErrorCode> kFsCodeMap = {
      {spis::ErrorCode::OK, errors::ErrorCode::OK},
      {spis::ErrorCode::INVALID_ARGUMENT,
       errors::ErrorCode::FS_INVALID_ARGUMENT},
      {spis::ErrorCode::DEADLINE_EXCEEDED,
       errors::ErrorCode::FS_DEADLINE_EXCEEDED},
      {spis::ErrorCode::NOT_FOUND, errors::ErrorCode::FS_NOT_FOUND},
      {spis::ErrorCode::INTERNAL_ERROR, errors::ErrorCode::FS_INTERNAL_ERROR},
      {spis::ErrorCode::UNAUTHENTICATED,
       errors::ErrorCode::FS_UNAUTHENTICATED}};

  auto iter = kFsCodeMap.find(fs_code);
  if (iter != kFsCodeMap.end()) {
    return iter->second;
  } else {
    return errors::ErrorCode::UNEXPECTED_ERROR;
  }
}

}  // namespace

HttpFeatureAdapter::HttpFeatureAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : FeatureAdapter(spec, service_id, party_id, feature_schema) {
  SERVING_ENFORCE(spec_.has_http_opts(), errors::ErrorCode::INVALID_ARGUMENT,
                  "invalid http options");

  // build feature fields
  int num_fields = feature_schema_->num_fields();
  for (int i = 0; i < num_fields; ++i) {
    const auto& f = feature_schema_->field(i);
    FeatureField f_field;
    f_field.set_type(DataTypeToFieldType(f->type()));
    f_field.set_name(f->name());
    feature_fields_.emplace_back(std::move(f_field));
  }

  const auto& http_opts = spec_.http_opts();

  // init channel
  channel_ = CreateBrpcChannel(
      http_opts.endpoint(), "http", http_opts.enable_lb(),
      http_opts.timeout_ms() > 0 ? http_opts.timeout_ms() : kTimeoutMs,
      http_opts.connect_timeout_ms() > 0 ? http_opts.connect_timeout_ms()
                                         : kConnectTimeoutMs,
      http_opts.has_tls_config() ? &http_opts.tls_config() : nullptr);
}

void HttpFeatureAdapter::OnFetchFeature(const Request& request,
                                        Response* response) {
  auto request_body = SerializeRequest(request);

  yacl::ElapsedTimer timer;

  brpc::Controller cntl;
  cntl.http_request().uri() = spec_.http_opts().endpoint();
  cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
  cntl.http_request().set_content_type("application/json");
  cntl.request_attachment().append(request_body);
  channel_->CallMethod(NULL, &cntl, NULL, NULL, NULL);
  SERVING_ENFORCE(!cntl.Failed(), errors::ErrorCode::NETWORK_ERROR,
                  "http request failed, endpoint:{}, detail:{}",
                  spec_.http_opts().endpoint(), cntl.ErrorText());

  DeserializeResponse(cntl.response_attachment().to_string(), response);
}

std::string HttpFeatureAdapter::SerializeRequest(const Request& request) {
  spis::BatchFetchFeatureRequest batch_request;

  batch_request.mutable_header()->mutable_data()->insert(
      request.header->data().begin(), request.header->data().end());
  batch_request.set_model_service_id(service_id_);
  batch_request.set_party_id(party_id_);
  batch_request.mutable_feature_fields()->Assign(feature_fields_.begin(),
                                                 feature_fields_.end());
  batch_request.mutable_param()->CopyFrom(*(request.fs_param));

  std::string json_str;
  ::google::protobuf::util::JsonOptions options;
  options.preserve_proto_field_names = true;
  auto status = google::protobuf::util::MessageToJsonString(batch_request,
                                                            &json_str, options);
  if (!status.ok()) {
    SERVING_THROW(errors::ErrorCode::SERIALIZE_FAILED,
                  "serialize fetch feature request failed: {}",
                  status.ToString());
  }

  return json_str;
}

void HttpFeatureAdapter::DeserializeResponse(const std::string& res_context,
                                             Response* response) {
  spis::BatchFetchFeatureResponse batch_response;
  auto status = ::google::protobuf::util::JsonStringToMessage(res_context,
                                                              &batch_response);
  SERVING_ENFORCE(status.ok(), errors::ErrorCode::DESERIALIZE_FAILED,
                  "deserialize response context({}) failed: {}", res_context,
                  status.ToString());
  SERVING_ENFORCE(batch_response.status().code() == spis::ErrorCode::OK,
                  MappingErrorCode(batch_response.status().code()),
                  "fetch features response error, msg: {}, code: {}",
                  batch_response.status().msg(),
                  batch_response.status().code());
  SERVING_ENFORCE(!batch_response.features().empty(),
                  errors::ErrorCode::IO_ERROR, "get empty features.");

  response->header->mutable_data()->swap(
      *batch_response.mutable_header()->mutable_data());
  response->features =
      FeaturesToTable(batch_response.features(), feature_schema_);
}

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kHttpOpts,
                 HttpFeatureAdapter);

}  // namespace secretflow::serving::feature
