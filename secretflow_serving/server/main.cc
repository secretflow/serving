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
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/server/kuscia/config_parser.h"
#include "secretflow_serving/server/server.h"
#include "secretflow_serving/server/trace/trace.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/util/network.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/config/serving_config.pb.h"
#include "secretflow_serving/config/trace_config.pb.h"

DEFINE_string(config_mode, "",
              "config mode for serving, default value will use the raw config "
              "defined. optional value: kuscia");
DEFINE_string(serving_config_file, "",
              "read an ascii config protobuf from the supplied file name.");

DEFINE_bool(enable_peers_load_balancer, false,
            "whether to enable load balancer between parties");

// logging config
DEFINE_string(
    logging_config_file, "",
    "read an ascii LoggingConfig protobuf from the supplied file name.");

// trace config
DEFINE_string(
    trace_config_file, "",
    "read an ascii TraceConfig protobuf from the supplied file name.");

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

void InitTracer() {
  secretflow::serving::TraceConfig trace_log;
  if (!FLAGS_trace_config_file.empty()) {
    auto trace_config_str =
        secretflow::serving::ReadFileContent(FLAGS_trace_config_file);
    if (!secretflow::serving::CheckContentEmpty(trace_config_str)) {
      secretflow::serving::JsonToPb(
          secretflow::serving::UnescapeJson(trace_config_str), &trace_log);
    }
  }
  secretflow::serving::InitTracer(trace_log);
}

int main(int argc, char* argv[]) {
  // Initialize the symbolizer to get a human-readable stack trace
  absl::InitializeSymbolizer(argv[0]);

  gflags::SetVersionString(SERVING_VERSION_STRING);
  gflags::AllowCommandLineReparsing();
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  try {
    InitLogger();
    InitTracer();

    SPDLOG_INFO("version: {}", SERVING_VERSION_STRING);

    {
      auto op_def_list =
          secretflow::serving::op::OpFactory::GetInstance()->GetAllOps();
      std::vector<std::string> op_names;
      std::for_each(
          op_def_list.begin(), op_def_list.end(),
          [&](const std::shared_ptr<const secretflow::serving::op::OpDef>& o) {
            op_names.emplace_back(o->name());
          });
      SPDLOG_INFO("op list: {}",
                  fmt::join(op_names.begin(), op_names.end(), ", "));
    }

    STRING_EMPTY_VALIDATOR(FLAGS_serving_config_file);

    // init server options
    secretflow::serving::Server::Options server_opts;
    if (FLAGS_config_mode == "kuscia") {
      secretflow::serving::kuscia::KusciaConfigParser config_parser(
          FLAGS_serving_config_file);
      server_opts.server_config = config_parser.server_config();
      server_opts.cluster_config = config_parser.cluster_config();
      server_opts.model_config = config_parser.model_config();
      server_opts.feature_source_config = config_parser.feature_config();
      server_opts.service_id = config_parser.service_id();
    } else {
      secretflow::serving::ServingConfig serving_conf;
      LoadPbFromJsonFile(FLAGS_serving_config_file, &serving_conf);

      server_opts.server_config = serving_conf.server_conf();
      server_opts.cluster_config = serving_conf.cluster_conf();
      server_opts.model_config = serving_conf.model_conf();
      if (serving_conf.has_feature_source_conf()) {
        server_opts.feature_source_config = serving_conf.feature_source_conf();
      }
      server_opts.service_id = serving_conf.id();
    }

    auto clannels = secretflow::serving::BuildChannelsFromConfig(
        server_opts.cluster_config, FLAGS_enable_peers_load_balancer);
    secretflow::serving::Server server(std::move(server_opts));
    server.Start(std::move(clannels));

    server.WaitForEnd();
  } catch (const secretflow::serving::Exception& e) {
    // TODO: custom status sink
    SPDLOG_ERROR("server startup failed, code: {}, msg: {}, stack: {}",
                 e.code(), e.what(), e.stack_trace());
    return -1;
  } catch (const std::exception& e) {
    // TODO: custom status sink
    SPDLOG_ERROR("server startup failed, msg:{}", e.what());
    return -1;
  }

  return 0;
}
