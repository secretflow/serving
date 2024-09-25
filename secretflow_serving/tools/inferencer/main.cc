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

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "gflags/gflags.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/core/logging.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/tools/inferencer/inference_executor.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/config/serving_config.pb.h"

DEFINE_string(serving_config_file, "",
              "read an ascii config protobuf from the supplied file name.");
DEFINE_string(inference_config_file, "", "");

// logging config
DEFINE_string(
    logging_config_file, "",
    "read an ascii LoggingConfig protobuf from the supplied file name.");

DECLARE_bool(inferencer_mode);

#define STRING_EMPTY_VALIDATOR(str_config)                                  \
  if (str_config.empty()) {                                                 \
    SERVING_THROW(secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT, \
                  "{} get empty value", #str_config);                       \
  }

void InitLogger() {
  secretflow::serving::LoggingConfig log_config;
  if (!FLAGS_logging_config_file.empty()) {
    auto logging_config_str =
        secretflow::serving::ReadFileContent(FLAGS_logging_config_file);
    if (!secretflow::serving::CheckContentEmpty(logging_config_str)) {
      secretflow::serving::JsonToPb(
          secretflow::serving::UnescapeJson(logging_config_str), &log_config);
    }
  }
  secretflow::serving::SetupLogging(log_config);
}

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  absl::InitializeSymbolizer(argv[0]);

  gflags::SetVersionString(SERVING_VERSION_STRING);
  gflags::AllowCommandLineReparsing();
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  FLAGS_inferencer_mode = true;

  try {
    InitLogger();

    SPDLOG_INFO("version: {}", SERVING_VERSION_STRING);

    STRING_EMPTY_VALIDATOR(FLAGS_serving_config_file);
    STRING_EMPTY_VALIDATOR(FLAGS_inference_config_file);

    // init server options
    secretflow::serving::ServingConfig serving_conf;
    secretflow::serving::LoadPbFromJsonFile(FLAGS_serving_config_file,
                                            &serving_conf);

    secretflow::serving::tools::InferenceConfig inference_conf;
    secretflow::serving::LoadPbFromJsonFile(FLAGS_inference_config_file,
                                            &inference_conf);

    secretflow::serving::tools::InferenceExecutor server(
        secretflow::serving::tools::InferenceExecutor::Options{
            .serving_conf = std::move(serving_conf),
            .inference_conf = std::move(inference_conf)});
    server.Run();
  } catch (const secretflow::serving::Exception& e) {
    std::string msg =
        fmt::format("inferencer run failed, code: {}, msg: {}, stack: {}",
                    e.code(), e.what(), e.stack_trace());
    SPDLOG_ERROR(msg);
    std::cerr << msg << std::endl;
    return -1;
  } catch (const std::exception& e) {
    std::string msg = fmt::format("inferencer run failed, msg:{}", e.what());
    SPDLOG_ERROR(msg);
    std::cerr << msg << std::endl;
    return -1;
  }

  return 0;
}
