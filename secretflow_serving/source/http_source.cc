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
#include "brpc/uri.h"
#include "fmt/ranges.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/source/factory.h"
#include "secretflow_serving/util/network.h"

#include "secretflow_serving/config/model_config.pb.h"

namespace secretflow::serving {

namespace {

void CopyQuery(const brpc::URI& source, brpc::URI& dst) {
  for (auto it = source.QueryBegin(); it != source.QueryEnd(); ++it) {
    dst.SetQuery(it->first, it->second);
  }
}

// ref: https://www.rfc-editor.org/rfc/rfc3986#section-5.2.3
// since we only merge relative path, suffix cannot starts with '/'
std::string MergePath(const std::string& prefix, const std::string& suffix) {
  static const std::string sep = "/";

  if (suffix.empty()) {
    return prefix;
  }

  if (prefix.empty()) {
    return sep + suffix;
  }

  auto prefix_sep_iter =
      std::find_end(prefix.begin(), prefix.end(), sep.begin(), sep.end());
  if (prefix_sep_iter == prefix.end()) {
    return suffix;
  }

  return std::string(prefix.begin(), prefix_sep_iter) + sep + suffix;
}

std::string GetUriStr(const brpc::URI& uri) {
  std::stringstream oss;
  uri.Print(oss);
  SERVING_ENFORCE(oss.good(), errors::ErrorCode::IO_ERROR,
                  "serialize target uri failed, (host:{}, port:{}, path:{}, "
                  "query_count:{}, fragment:{})",
                  uri.host(), uri.port(), uri.path(), uri.QueryCount(),
                  uri.fragment());
  return oss.str();
}

// ref: https://www.rfc-editor.org/rfc/rfc3986#section-5.2.2
std::string UriRelativeResolutionImpl(const std::string& origin_path,
                                      const brpc::URI& redirect_uri,
                                      bool is_relative_path = false) {
  brpc::URI base_uri;

  SERVING_ENFORCE(base_uri.SetHttpURL(origin_path) == 0,
                  errors::ErrorCode::NETWORK_ERROR,
                  "parse origin path failed, origin: {}", origin_path);

  if (!redirect_uri.host().empty()) {
    // fragment must be preserved according to:
    // https://www.rfc-editor.org/rfc/rfc9110.html#section-10.2.2
    if (redirect_uri.fragment().empty() && !base_uri.fragment().empty()) {
      return GetUriStr(redirect_uri) + '#' + base_uri.fragment();
    }
    return GetUriStr(redirect_uri);
  }

  brpc::URI target_uri;
  target_uri.set_scheme(base_uri.scheme());
  target_uri.set_host(base_uri.host());
  target_uri.set_port(base_uri.port());

  if (is_relative_path) {
    // drop '/' of redirect_uri path we add before
    target_uri.set_path(
        MergePath(base_uri.path(), redirect_uri.path().substr(1)));
  } else {
    if (redirect_uri.path().empty()) {
      target_uri.set_path(base_uri.path());
      if (redirect_uri.QueryCount() == 0) {
        CopyQuery(base_uri, target_uri);
      } else {
        CopyQuery(redirect_uri, target_uri);
      }
    } else {
      target_uri.set_path(redirect_uri.path());
      CopyQuery(redirect_uri, target_uri);
    }
  }

  auto target_uri_str = GetUriStr(target_uri);
  if (!redirect_uri.fragment().empty()) {
    target_uri_str += "#" + redirect_uri.fragment();
  } else {
    if (!base_uri.fragment().empty()) {
      // fragment must be preserved according to:
      // https://www.rfc-editor.org/rfc/rfc9110.html#section-10.2.2
      target_uri_str += "#" + base_uri.fragment();
    }
  }
  return target_uri_str;
}

}  // namespace

// reference: https://www.rfc-editor.org/rfc/rfc3986#section-5.2
std::string UriRelativeResolution(const std::string& origin_path,
                                  const std::string& redirect_path) {
  // relative means redirect_path should be merged with origin_path
  bool is_relative_path = false;
  brpc::URI redirect_uri;
  auto new_redirect_path = redirect_path;
  // ref: https://www.rfc-editor.org/rfc/rfc3986#section-4.2
  // if starts with //, then it is followed by: authority path-abempty
  if (redirect_path.find("//") == 0) {
    new_redirect_path = redirect_path.substr(2);
  } else if (redirect_path.find("://") == std::string::npos &&
             redirect_path[0] != '/') {
    is_relative_path = true;
    // add / for brpc URI parser, otherwise first part of path will be treat as
    // host
    new_redirect_path = '/' + redirect_path;
  }
  SERVING_ENFORCE(redirect_uri.SetHttpURL(new_redirect_path) == 0,
                  errors::ErrorCode::NETWORK_ERROR,
                  "parse uri failed, path: {}", new_redirect_path);
  return UriRelativeResolutionImpl(origin_path, redirect_uri, is_relative_path);
}

HttpSource::HttpSource(const ModelConfig& config, const std::string& service_id)
    : Source(config, service_id) {
  SERVING_ENFORCE(!config.source_path().empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "http source endpoint is empty");

  endpoint_ = config.source_path();
  SPDLOG_INFO("Model {} source endpoint is {}", service_id_, endpoint_);

  if (config.has_http_source_meta()) {
    auto& http_opts = config.http_source_meta();
    if (http_opts.timeout_ms() > 0) {
      http_timeout_ms_ = http_opts.timeout_ms();
    }
    if (http_opts.connect_timeout_ms() > 0) {
      connect_timeout_ms_ = http_opts.connect_timeout_ms();
    }
    if (http_opts.has_tls_config()) {
      tls_config_ = std::make_unique<TlsConfig>(http_opts.tls_config());
    }
  }
}

void HttpSource::OnPullModel(const std::string& dst_path) {
  auto cur_url_path = endpoint_;

  const static std::unordered_set<int> kRedirectCodes = {301, 302, 303, 307,
                                                         308};
  std::unordered_set<std::string> redirected_paths;
  brpc::Controller cntl;

  while (true) {
    SERVING_ENFORCE(redirected_paths.insert(cur_url_path).second,
                    errors::ErrorCode::NETWORK_ERROR,
                    "loop redirect cur_url_path:{} (paths: {})", cur_url_path,
                    fmt::join(redirected_paths, ", "));
    cntl.Reset();

    // init channel
    auto channel =
        CreateBrpcChannel(cur_url_path, "http", false, http_timeout_ms_,
                          connect_timeout_ms_, tls_config_.get());

    cntl.http_request().uri() = cur_url_path;
    cntl.http_request().set_method(brpc::HTTP_METHOD_GET);
    channel->CallMethod(NULL, &cntl, NULL, NULL, NULL);

    if (!cntl.Failed()) {
      break;
    }

    if (kRedirectCodes.count(cntl.http_response().status_code())) {
      auto* location = cntl.http_response().GetHeader("Location");
      SERVING_ENFORCE(location != nullptr && !location->empty(),
                      errors::ErrorCode::NETWORK_ERROR,
                      "http redirect but no Location header");
      cur_url_path = UriRelativeResolution(cur_url_path, *location);
      SPDLOG_INFO("redirect to {}, http code: {}", cur_url_path,
                  cntl.http_response().status_code());
      continue;
    }

    SERVING_THROW(errors::ErrorCode::NETWORK_ERROR,
                  "http request failed, endpoint_:{}, detail:{}", cur_url_path,
                  cntl.ErrorText());
  }

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
