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

#include "secretflow_serving/core/logging.h"

#include <filesystem>

#include "butil/logging.h"
#include "fmt/format.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

namespace {
// clang-format off
/// custom formatting:
/// https://github.com/gabime/spdlog/wiki/3.-Custom-formatting
/// example: ```2019-11-29 06:58:54.633 [info] [log_test.cpp:TestBody:16] The answer is 42.```
// clang-format on
const char* kFormatPattern = "%Y-%m-%d %H:%M:%S.%e [%l] [%s:%!:%#] %v";

const char* kBrpcUnknownFuncName = "BRPC";

spdlog::level::level_enum FromBrpcLogSeverity(int severity) {
  spdlog::level::level_enum level = spdlog::level::off;
  if (severity == ::logging::BLOG_INFO) {
    level = spdlog::level::debug;
  } else if (severity == ::logging::BLOG_NOTICE) {
    level = spdlog::level::info;
  } else if (severity == ::logging::BLOG_WARNING) {
    level = spdlog::level::warn;
  } else if (severity == ::logging::BLOG_ERROR) {
    level = spdlog::level::err;
  } else if (severity == ::logging::BLOG_FATAL) {
    level = spdlog::level::critical;
  } else {
    level = spdlog::level::warn;
  }
  return level;
}

spdlog::level::level_enum FromServingLogLevel(LogLevel serving_log_level) {
  spdlog::level::level_enum level = spdlog::level::off;
  if (serving_log_level == LogLevel::DEBUG_LOG_LEVEL) {
    level = spdlog::level::debug;
  } else if (serving_log_level == LogLevel::INFO_LOG_LEVEL) {
    level = spdlog::level::info;
  } else if (serving_log_level == LogLevel::WARN_LOG_LEVEL) {
    level = spdlog::level::warn;
  } else if (serving_log_level == LogLevel::ERROR_LOG_LEVEL) {
    level = spdlog::level::err;
  } else {
    level = spdlog::level::info;
  }
  return level;
}

class ServingLogSink : public ::logging::LogSink {
 public:
  bool OnLogMessage(int severity, const char* file, int line,
                    const butil::StringPiece& log_content) override {
    spdlog::level::level_enum log_level = FromBrpcLogSeverity(severity);
    spdlog::log(spdlog::source_loc{file, line, kBrpcUnknownFuncName}, log_level,
                "{}", fmt::string_view(log_content.data(), log_content.size()));
    return true;
  }
};

void SinkBrpcLogWithDefaultLogger() {
  static ServingLogSink log_sink;
  ::logging::SetLogSink(&log_sink);
  ::logging::SetMinLogLevel(::logging::BLOG_ERROR);
}
}  // namespace

void SetupLogging(const LoggingConfig& config) {
  spdlog::level::level_enum level = FromServingLogLevel(config.log_level());

  auto system_log_path = config.system_log_path().empty()
                             ? "serving.log"
                             : config.system_log_path();
  auto max_log_file_size = config.max_log_file_size() > 0
                               ? config.max_log_file_size()
                               : 500 * 1024 * 1024;
  auto max_log_file_count =
      config.max_log_file_count() > 0 ? config.max_log_file_count() : 10;

  auto log_dir = std::filesystem::path(system_log_path).parent_path();
  if (!log_dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    SERVING_ENFORCE(ec.value() == 0, errors::ErrorCode::IO_ERROR,
                    "failed to create dir={}, reason = {}", log_dir.string(),
                    ec.message());
  }

  std::vector<spdlog::sink_ptr> sinks;
  {
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        system_log_path, max_log_file_size, max_log_file_count);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.emplace_back(std::move(file_sink));
    sinks.emplace_back(std::move(console_sink));
  }

  auto root_logger = std::make_shared<spdlog::logger>(
      "system_root", sinks.begin(), sinks.end());
  root_logger->set_level(level);
  root_logger->set_pattern(kFormatPattern);
  root_logger->flush_on(level);
  spdlog::set_default_logger(root_logger);

  SinkBrpcLogWithDefaultLogger();
}

}  // namespace secretflow::serving
