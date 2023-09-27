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

#include "secretflow_serving/server/metrics/metrics_service.h"

//
#include <butil/iobuf.h>
//
#include <brpc/builtin/prometheus_metrics_service.h>
#include <brpc/channel.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <brpc/server.h>
#include <prometheus/serializer.h>
#include <prometheus/text_serializer.h>

#include <stdexcept>

#include "gtest/gtest.h"
#include "yacl/utils/elapsed_timer.h"

namespace secretflow::serving::metrics {

class ServingMetricsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto metrics_service = new metrics::MetricsService();
    // metrics_service->RegisterCollectable(metrics::GetDefaultRegistry());
    if (server_.AddService(metrics_service, brpc::SERVER_OWNS_SERVICE) != 0) {
      throw std::runtime_error("add service failed.");
    }
    brpc::ServerOptions metrics_server_options;
    if (server_.Start("127.0.0.1:0", &metrics_server_options) != 0) {
      throw std::runtime_error("start server failed.");
    }
    server_addr_ = butil::endpoint2str(server_.listen_address()).c_str();
  }
  void TearDown() override {
    server_.Stop(0);
    server_.Join();
  }

 protected:
  brpc::Server server_;
  std::string server_addr_;
};

TEST_F(ServingMetricsTest, basic_test) {
  brpc::ChannelOptions options;
  options.protocol = "http";
  auto channel = std::make_unique<brpc::Channel>();
  channel->Init(server_addr_.c_str(), "", &options);

  brpc::Controller cntl;
  cntl.ignore_eovercrowded();
  cntl.http_request().uri() = server_addr_ + "/metrics";

  channel->CallMethod(nullptr, &cntl, nullptr, nullptr, nullptr);
  auto first_transfer_size = cntl.response_attachment().size();

  cntl.Reset();
  cntl.ignore_eovercrowded();
  cntl.http_request().uri() = server_addr_ + "/metrics";
  channel->CallMethod(nullptr, &cntl, nullptr, nullptr, nullptr);

  std::stringstream ss;
  size_t metrics_tranfer_size = 0;
  ss << cntl.response_attachment().to_string();
  std::string_view transfer_label{"exposer_transferred_bytes_total"};
  while (!ss.eof()) {
    std::string line;
    std::getline(ss, line);
    if (std::string_view(line)
            .substr(0, transfer_label.size())
            .compare(transfer_label) == 0) {
      metrics_tranfer_size = stoull(line.substr(transfer_label.size() + 1));
      break;
    }
  }

  EXPECT_EQ(metrics_tranfer_size, first_transfer_size);
}

}  // namespace secretflow::serving::metrics
