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

#include "secretflow_serving/util/retry_policy.h"

#include "brpc/channel.h"
#include "brpc/http_status_code.h"
#include "brpc/server.h"
#include "gtest/gtest.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/backoff_policy.h"

#include "secretflow_serving/source/http_service.pb.h"

namespace secretflow::serving {

TEST(BackOffTest, Fixed) {
  FixedBackOffPolicy policy;
  EXPECT_EQ(policy.GetBackOffMs(0), 10);
  EXPECT_EQ(policy.GetBackOffMs(1), 10);

  FixedBackOffConfig config;
  config.set_interval_ms(100);
  FixedBackOffPolicy policy2(&config);
  EXPECT_EQ(policy2.GetBackOffMs(0), 100);
  EXPECT_EQ(policy2.GetBackOffMs(1), 100);
}

TEST(BackOffTest, Random) {
  RandomBackOffPolicy policy;
  EXPECT_GE(policy.GetBackOffMs(0), 10);
  EXPECT_LE(policy.GetBackOffMs(1), 50);

  RandomBackOffConfig config;
  config.set_min_ms(100);
  config.set_max_ms(200);
  RandomBackOffPolicy policy2(&config);
  EXPECT_GE(policy2.GetBackOffMs(0), 100);
  EXPECT_LE(policy2.GetBackOffMs(1), 200);
}

TEST(BackOffTest, Exponential) {
  ExponentialBackOffPolicy policy;
  EXPECT_EQ(policy.GetBackOffMs(0), 10);
  EXPECT_EQ(policy.GetBackOffMs(1), 20);
  EXPECT_EQ(policy.GetBackOffMs(2), 40);

  ExponentialBackOffConfig config;
  config.set_factor(3);
  config.set_init_ms(10);
  ExponentialBackOffPolicy policy2(&config);
  EXPECT_EQ(policy2.GetBackOffMs(0), 10);
  EXPECT_EQ(policy2.GetBackOffMs(1), 30);
}

class MockHttpSource : public HttpService {
 public:
  MockHttpSource(int http_code, int error_cnt = 1)
      : http_code_(http_code), error_cnt_(error_cnt) {}

  void default_method(google::protobuf::RpcController* cntl_base,
                      const HttpRequest* /*request*/,
                      HttpResponse* /*response*/,
                      google::protobuf::Closure* done) {
    brpc::ClosureGuard done_guard(done);
    brpc::Controller* cntl = static_cast<brpc::Controller*>(cntl_base);
    SPDLOG_INFO("receive request from client: {}",
                butil::endpoint2str(cntl->remote_side()).c_str());
    std::ostringstream oss;
    cntl->http_request().uri().Print(oss);
    SPDLOG_INFO("request url: {}", oss.str());
    SPDLOG_INFO("request path: {}", cntl->http_request().unresolved_path());

    if (error_cnt_) {
      error_cnt_--;
      cntl->http_response().set_status_code(http_code_);
    } else {
      cntl->http_response().set_status_code(brpc::HTTP_STATUS_OK);
    }
  }

 private:
  int http_code_;
  int error_cnt_;
};

TEST(RetryPolicyTest, ConfigTest) {
  auto poilcy1 =
      RetryPolicyFactory::GetInstance()->GetRetryPolicyInfo("no_exist");
  EXPECT_EQ(3, poilcy1.max_retry_count);
  auto poilcy2 =
      RetryPolicyFactory::GetInstance()->GetRetryPolicyInfo("no_exist2");
  EXPECT_EQ(poilcy1.policy.get(), poilcy2.policy.get());

  RetryPolicyConfig config;
  config.set_max_retry_count(4);
  RetryPolicyFactory::GetInstance()->SetConfig("test", &config);
  auto policy3 = RetryPolicyFactory::GetInstance()->GetRetryPolicyInfo("test");
  EXPECT_EQ(4, policy3.max_retry_count);
}

TEST(RetryPolicyTest, BackModeTest) {
  RetryPolicyConfig config;
  config.set_backoff_mode(RetryPolicyBackOffMode::EXPONENTIAL_BACKOFF);
  RetryPolicyFactory::GetInstance()->SetConfig("test1", &config);
  config.set_backoff_mode(RetryPolicyBackOffMode::RANDOM_BACKOFF);
  RetryPolicyFactory::GetInstance()->SetConfig("test2", &config);
}

void StartServerAddRequest(int http_code, int error_cnt, int retried_count,
                           bool rpc_ret = true) {
  RetryPolicyConfig config;
  config.set_retry_custom(true);
  config.set_retry_custom(true);
  RetryPolicyFactory::GetInstance()->SetConfig("test", &config);

  // start server
  auto source_service = std::make_unique<MockHttpSource>(http_code, error_cnt);
  brpc::ServerOptions server_options;
  brpc::Server server;
  if (server.AddService(source_service.get(), brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add service into brpc server.");
  }
  source_service.release();
  if (server.Start("127.0.0.1:0", &server_options) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "fail to start server");
  }
  std::string server_addr =
      butil::endpoint2str(server.listen_address()).c_str();
  SPDLOG_INFO("start serving server at {}", server_addr);

  brpc::ChannelOptions options;
  auto policy = RetryPolicyFactory::GetInstance()->GetRetryPolicyInfo("test");
  options.retry_policy = policy.policy.get();

  brpc::Channel channel;
  options.protocol = brpc::PROTOCOL_HTTP;  // or brpc::PROTOCOL_H2
  EXPECT_EQ(channel.Init(server_addr.c_str(), &options), 0);

  brpc::Controller cntl;
  EXPECT_EQ(3, policy.max_retry_count);
  cntl.http_request().uri() =
      server_addr + "/HttpService";  // 设置为待访问的URL
  channel.CallMethod(NULL, &cntl, NULL, NULL, NULL /*done*/);

  EXPECT_EQ(!cntl.Failed(), rpc_ret)
      << fmt::format("fail to call method: code: {}, msg: {}", cntl.ErrorCode(),
                     cntl.ErrorText());
  EXPECT_EQ(cntl.retried_count(), retried_count);

  // sleep to wait server ready
  sleep(2);

  server.Stop(0);
  server.Join();
}

TEST(RetryPolicyTest, OK) {
  StartServerAddRequest(brpc::HTTP_STATUS_OK, 0, 0, true);
}

TEST(RetryPolicyTest, CustomeCode) { StartServerAddRequest(502, 1, 1, true); }
TEST(RetryPolicyTest, Retry2) { StartServerAddRequest(502, 2, 2, true); }
TEST(RetryPolicyTest, Retry3) { StartServerAddRequest(502, 3, 3, true); }
TEST(RetryPolicyTest, RetryFail) { StartServerAddRequest(502, 4, 3, false); }

}  // namespace secretflow::serving
