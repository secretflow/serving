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

#include <string>
#include <string_view>
#include <utility>

#include "brpc/controller.h"
#include "opentelemetry/sdk/trace/tracer.h"

#include "secretflow_serving/apis/common.pb.h"
#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/apis/status.pb.h"
#include "secretflow_serving/config/trace_config.pb.h"

namespace secretflow::serving {

void InitTracer(const TraceConfig& trace_config);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
    std::string_view tracer_name = "", std::string_view version = "");

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateServerSpan(
    const brpc::Controller& cntl, const apis::Header& request_header,
    const std::string& span_info, std::string_view tracer_name = "");

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info = "unknown",
    ::google::protobuf::Map<std::string, std::string>* header_map = nullptr,
    std::string_view tracer_name = "");

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> CreateClientSpan(
    brpc::Controller* cntl, const std::string& service_info,
    apis::Header* request_header = nullptr, std::string_view tracer_name = "");

struct SpanAttrOption {
  int code = errors::ErrorCode::OK;
  std::string msg;
  std::string service_id;
  std::string party_id;
  const brpc::Controller* cntl = nullptr;
  bool is_client = false;
};

void SetSpanAttrs(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>& span,
    const SpanAttrOption& option);

inline std::string HexTraceId(const opentelemetry::trace::TraceId& trace) {
  char buf[32];
  trace.ToLowerBase16(buf);
  return std::string(buf, sizeof(buf));
}

}  // namespace secretflow::serving
