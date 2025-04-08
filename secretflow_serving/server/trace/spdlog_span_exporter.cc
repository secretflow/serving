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

#include "secretflow_serving/server/trace/spdlog_span_exporter.h"

#include <filesystem>

#include "opentelemetry/exporters/otlp/otlp_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/sdk/trace/span_data.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/utils.h"

#include "opentelemetry/proto/collector/trace/v1/trace_service.pb.h"
#include "opentelemetry/proto/trace/v1/trace.pb.h"
namespace secretflow::serving {

namespace {
const char* kFormatPattern = "%v";
}

SpdLogSpanExporter::SpdLogSpanExporter(const TraceLogConfig& trace_log_config)
    : logger_("trace_log") {
  auto trace_log_path = trace_log_config.trace_log_path().empty()
                            ? "trace.log"
                            : trace_log_config.trace_log_path();
  auto max_log_file_size = trace_log_config.max_trace_log_file_size() > 0
                               ? trace_log_config.max_trace_log_file_size()
                               : 500 * 1024 * 1024;
  auto max_log_file_count = trace_log_config.max_trace_log_file_count() > 0
                                ? trace_log_config.max_trace_log_file_count()
                                : 10;
  auto log_dir = std::filesystem::path(trace_log_path).parent_path();
  if (!log_dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    SERVING_ENFORCE(ec.value() == 0, errors::ErrorCode::IO_ERROR,
                    "failed to create dir={}, reason = {}", log_dir.string(),
                    ec.message());
  }

  auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      trace_log_path, max_log_file_size, max_log_file_count);
  logger_.sinks().push_back(file_sink);
  logger_.set_level(spdlog::level::info);
  logger_.set_pattern(kFormatPattern);
  SPDLOG_INFO("trace logger init success, trace_log_path={}", trace_log_path);
}

std::unique_ptr<opentelemetry::sdk::trace::Recordable>
SpdLogSpanExporter::MakeRecordable() noexcept {
  return std::make_unique<opentelemetry::exporter::otlp::OtlpRecordable>();
}

opentelemetry::sdk::common::ExportResult SpdLogSpanExporter::Export(
    const opentelemetry::nostd::span<
        std::unique_ptr<opentelemetry::sdk::trace::Recordable>>&
        spans) noexcept {
  if (isShutdown()) {
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  try {
    opentelemetry::proto::collector::trace::v1::ExportTraceServiceRequest
        request;
    opentelemetry::exporter::otlp::OtlpRecordableUtils::PopulateRequest(
        spans, &request);
    for (const auto& resource_spans : request.resource_spans()) {
      logger_.info(PbToJson(&resource_spans));
    }
    logger_.flush();
  } catch (const std::exception& e) {
    SPDLOG_ERROR("log span failed, unexcept error={}", e.what());
    return opentelemetry::sdk::common::ExportResult::kFailure;
  } catch (...) {
    SPDLOG_ERROR("log span failed, unknown error");
    return opentelemetry::sdk::common::ExportResult::kFailure;
  }

  return opentelemetry::sdk::common::ExportResult::kSuccess;
}

bool SpdLogSpanExporter::Shutdown(
    std::chrono::microseconds /*timeout*/) noexcept {
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  logger_.flush();
  is_shutdown_ = true;
  return true;
}

bool SpdLogSpanExporter::isShutdown() const noexcept {
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  return is_shutdown_;
}

bool SpdLogSpanExporter::ForceFlush(
    std::chrono::microseconds /*timeout*/) noexcept {
  const std::lock_guard<opentelemetry::common::SpinLockMutex> locked(lock_);
  logger_.flush();
  return true;
}

}  // namespace secretflow::serving
