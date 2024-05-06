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

#include "secretflow_serving/source/http_source.h"

#include <fstream>

#include "brpc/controller.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/source/factory.h"
#include "secretflow_serving/util/network.h"

#include "secretflow_serving/config/model_config.pb.h"

namespace secretflow::serving {

namespace {

constexpr size_t kHttpTimeoutMs = 120 * 1000;
constexpr size_t kConnectTimeoutMs = 60 * 1000;
}  // namespace

HttpSource::HttpSource(const ModelConfig& config, const std::string& service_id)
    : Source(config, service_id) {
  SERVING_ENFORCE(!config.source_path().empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "http source path is empty");

  endpoint_ = config.source_path();
  SPDLOG_INFO("Model {} source path is {}", service_id_, endpoint_);

  auto http_timeout_ms = kHttpTimeoutMs;
  auto connect_timeout_ms = kConnectTimeoutMs;
  const TlsConfig* tls_config = nullptr;

  if (config.has_http_source_meta()) {
    auto& http_opts = config.http_source_meta();
    if (http_opts.timeout_ms() > 0) {
      http_timeout_ms = http_opts.timeout_ms();
    }
    if (http_opts.connect_timeout_ms() > 0) {
      connect_timeout_ms = http_opts.connect_timeout_ms();
    }
    if (http_opts.has_tls_config()) {
      tls_config = &http_opts.tls_config();
    }
  }

  // init channel
  channel_ = CreateBrpcChannel(endpoint_, "http", false, http_timeout_ms,
                               connect_timeout_ms, tls_config);
}

void HttpSource::OnPullModel(const std::string& dst_path) {
  // just copy file
  brpc::Controller cntl;
  cntl.http_request().uri() = endpoint_;
  cntl.http_request().set_method(brpc::HTTP_METHOD_GET);
  channel_->CallMethod(NULL, &cntl, NULL, NULL, NULL);
  SERVING_ENFORCE(!cntl.Failed(), errors::ErrorCode::NETWORK_ERROR,
                  "http request failed, endpoint_:{}, detail:{}", endpoint_,
                  cntl.ErrorText());

  auto& response = cntl.response_attachment();

  std::ofstream outfile(dst_path, std::ios::binary | std::ios::trunc);
  SERVING_ENFORCE(outfile.is_open(), errors::ErrorCode::IO_ERROR,
                  "open file {} faliled", dst_path);
  outfile << response;
  SPDLOG_INFO("download model file from {} to {} successfully", endpoint_,
              dst_path);
}

REGISTER_SOURCE(SourceType::ST_HTTP, HttpSource);

}  // namespace secretflow::serving
