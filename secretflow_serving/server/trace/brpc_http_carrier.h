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

#pragma once

#include "brpc/http_header.h"  // HttpHeader
#include "opentelemetry/context/propagation/text_map_propagator.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/server/trace/brpc_http_carrier.h"

#include "secretflow_serving/apis/common.pb.h"

namespace secretflow::serving {

class BrpcHttpCarrierGetable
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  BrpcHttpCarrierGetable(
      const brpc::HttpHeader& headers,
      const ::google::protobuf::Map<std::string, std::string>& header_map)
      : headers_(headers), header_map_(header_map) {}

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override {
    std::string key_data = key.data();
    const std::string* p = headers_.GetHeader(key_data);
    if (p) {
      SPDLOG_DEBUG("get header from http header, key: {}, value: {}", key_data,
                   *p);
      return *p;
    }

    auto iter = header_map_.find(key_data);
    if (iter != header_map_.end()) {
      SPDLOG_DEBUG("get header from http header, key: {}, value: {}", key_data,
                   iter->second);
      return iter->second;
    }

    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    SPDLOG_ERROR(
        "BrpcHttpCarrierGetable Set method should not be called, key: {}, "
        "value: {}",
        key, value);
  }

 private:
  const brpc::HttpHeader& headers_;
  const ::google::protobuf::Map<std::string, std::string>& header_map_;
};

class BrpcHttpCarrierSetable
    : public opentelemetry::context::propagation::TextMapCarrier {
 public:
  BrpcHttpCarrierSetable(
      brpc::HttpHeader* headers,
      ::google::protobuf::Map<std::string, std::string>* header_map)
      : headers_(headers), header_map_(header_map) {}

  opentelemetry::nostd::string_view Get(
      opentelemetry::nostd::string_view key) const noexcept override {
    SPDLOG_ERROR(
        "BrpcHttpCarrierSetable Get method should not be called, key: {}", key);
    return "";
  }

  void Set(opentelemetry::nostd::string_view key,
           opentelemetry::nostd::string_view value) noexcept override {
    if (headers_) {
      SPDLOG_DEBUG("set http header, key: {}, value: {}", key, value);

      headers_->SetHeader(std::string(key), std::string(value));
    }
    if (header_map_) {
      SPDLOG_DEBUG("set proto header, key: {}, value: {}", key, value);

      header_map_->insert({std::string(key), std::string(value)});
    }
  }

 private:
  brpc::HttpHeader* headers_;
  ::google::protobuf::Map<std::string, std::string>* header_map_;
};

}  // namespace secretflow::serving
