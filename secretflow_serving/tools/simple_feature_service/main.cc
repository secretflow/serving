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

#include <filesystem>

#include "brpc/server.h"
#include "fmt/format.h"
#include "gflags/gflags.h"
#include "google/protobuf/repeated_field.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/tools/simple_feature_service/simple_feature_service.h"
#include "secretflow_serving/util/arrow_helper.h"

#include "secretflow_serving/spis/batch_feature_service.pb.h"
#include "secretflow_serving/spis/error_code.pb.h"

DEFINE_uint32(port, 9999, "Port number, default: 9999");

DEFINE_string(csv_filename, "", "Filename");
DEFINE_string(csv_id_column_name, "", "column_name for id column");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  SPDLOG_INFO("starting service at port: {}", FLAGS_port);

  SERVING_ENFORCE(!FLAGS_csv_filename.empty(),
                  secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT,
                  "csv_filename cmdline flags should be specified");
  SERVING_ENFORCE(std::filesystem::exists(FLAGS_csv_filename),
                  secretflow::serving::errors::ErrorCode::FS_INVALID_ARGUMENT,
                  "{} is not exist.", FLAGS_csv_filename);
  SERVING_ENFORCE(!FLAGS_csv_id_column_name.empty(),
                  secretflow::serving::errors::ErrorCode::INVALID_ARGUMENT,
                  "csv_id_column_name cmdline flags should be specified");
  SPDLOG_INFO("csv_filename: {}", FLAGS_csv_filename);
  SPDLOG_INFO("csv_id_column_name: {}", FLAGS_csv_id_column_name);

  auto feature_sevice =
      std::make_unique<secretflow::serving::SimpleBatchFeatureService>(
          FLAGS_csv_filename, FLAGS_csv_id_column_name);

  brpc::Server server;
  if (server.AddService(feature_sevice.get(), brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(secretflow::serving::errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add feature_sevice into brpc server.");
  } else {
    feature_sevice.release();
  }

  brpc::ServerOptions server_options;
  if (server.Start(FLAGS_port, &server_options) != 0) {
    SERVING_THROW(secretflow::serving::errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to start brpc server at port:{}", FLAGS_port);
  }

  SPDLOG_INFO("simple_feature_service started at port: {}", FLAGS_port);

  server.RunUntilAskedToQuit();
  server.Stop(0);
  server.Join();
}