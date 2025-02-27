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
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/server/trace/trace.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/network.h"
#include "secretflow_serving/util/retry_policy.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/apis/status.pb.h"
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
  auto channel_name = http_opts.endpoint();
  channel_ = CreateBrpcChannel(
      channel_name, http_opts.endpoint(), "http", http_opts.enable_lb(),
      http_opts.timeout_ms() > 0 ? http_opts.timeout_ms() : kTimeoutMs,
      http_opts.connect_timeout_ms() > 0 ? http_opts.connect_timeout_ms()
                                         : kConnectTimeoutMs,
      http_opts.has_tls_config() ? &http_opts.tls_config() : nullptr,
      http_opts.has_retry_policy_config() ? &http_opts.retry_policy_config()
                                          : nullptr);
}

void HttpFeatureAdapter::OnFetchFeature(const Request& request,
                                        Response* response) {
  brpc::Controller cntl;
  cntl.http_request().uri() = spec_.http_opts().endpoint();
  cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
  cntl.http_request().set_content_type("application/json");

  auto service_info = fmt::format("{}: {}", "HttpFeatureAdapter/OnFetchFeature",
                                  spec_.http_opts().endpoint());
  spis::Header trace_header;
  auto span =
      CreateClientSpan(&cntl, service_info, trace_header.mutable_data());
  auto spi_request = MakeSpiRequest(trace_header, request);

  cntl.request_attachment().append(spi_request);

  SpanAttrOption span_option;
  span_option.code = errors::ErrorCode::OK;
  span_option.msg = fmt::format("fetch_feature from {} successfully",
                                spec_.http_opts().endpoint());
  span_option.cntl = &cntl;
  span_option.is_client = true;
  span_option.party_id = party_id_;
  span_option.service_id = service_id_;

  spis::BatchFetchFeatureResponse spi_response;

  channel_->CallMethod(NULL, &cntl, NULL, NULL, NULL);
  if (cntl.Failed()) {
    span_option.code = errors::ErrorCode::NETWORK_ERROR;
    span_option.msg = fmt::format(
        "http request failed, endpoint:{}, request: {}, error info detail:{}",
        spec_.http_opts().endpoint(), spi_request, cntl.ErrorText());
  } else {
    auto status = ::google::protobuf::util::JsonStringToMessage(
        cntl.response_attachment().to_string(), &spi_response);
    if (!status.ok()) {
      span_option.code = errors::ErrorCode::DESERIALIZE_FAILED;
      span_option.msg = fmt::format(
          "deserialize response context failed: request: {}, error: {}",
          spi_request, status.ToString());
    } else if (spi_response.status().code() != spis::ErrorCode::OK) {
      span_option.code = MappingErrorCode(spi_response.status().code());
      span_option.msg = fmt::format(
          "fetch features response error, request: {}, msg: {}, code: {}",
          spi_request, spi_response.status().msg(),
          spi_response.status().code());
    } else if (spi_response.features().empty()) {
      span_option.code = errors::ErrorCode::IO_ERROR;
      span_option.msg =
          fmt::format("get empty features, request: {}", spi_request);
    }
  }

  SetSpanAttrs(span, span_option);

  SERVING_ENFORCE(span_option.code == errors::ErrorCode::OK, span_option.code,
                  "{}", span_option.msg);
  response->header->mutable_data()->swap(
      *spi_response.mutable_header()->mutable_data());
  response->features =
      FeaturesToRecordBatch(spi_response.features(), feature_schema_);
}

std::string HttpFeatureAdapter::MakeSpiRequest(spis::Header& trace_header,
                                               const Request& request) {
  spis::BatchFetchFeatureRequest batch_request;

  batch_request.mutable_header()->Swap(&trace_header);
  batch_request.mutable_header()->mutable_data()->insert(
      request.header->data().begin(), request.header->data().end());
  batch_request.set_model_service_id(service_id_);
  batch_request.set_party_id(party_id_);
  batch_request.mutable_feature_fields()->Assign(feature_fields_.begin(),
                                                 feature_fields_.end());
  batch_request.mutable_param()->CopyFrom(*(request.fs_param));

  std::string json_str;
  ::google::protobuf::util::JsonPrintOptions options;
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

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kHttpOpts,
                 HttpFeatureAdapter);

}  // namespace secretflow::serving::feature
