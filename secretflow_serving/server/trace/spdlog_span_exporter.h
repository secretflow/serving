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

#include <memory>

#include "opentelemetry/common/spin_lock_mutex.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "spdlog/logger.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/config/trace_config.pb.h"

namespace secretflow::serving {

// export span to log file, one TracesData per line
// according to
// https://github.com/open-telemetry/opentelemetry-proto/blob/main/opentelemetry/proto/trace/v1/trace.proto
class SpdLogSpanExporter : public opentelemetry::sdk::trace::SpanExporter {
 public:
  explicit SpdLogSpanExporter(const TraceLogConfig& trace_log_config);

  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override;

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>>&
          spans) noexcept override;

  bool Shutdown(std::chrono::microseconds /*timeout*/) noexcept override;

  bool ForceFlush(std::chrono::microseconds /*timeout*/) noexcept override;

 private:
  [[nodiscard]] bool isShutdown() const noexcept;

 private:
  bool is_shutdown_ = false;

  mutable opentelemetry::common::SpinLockMutex lock_;

  spdlog::logger logger_;
};

}  // namespace secretflow::serving
