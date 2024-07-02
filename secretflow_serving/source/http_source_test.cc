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

#include "secretflow_serving/source/http_source.h"

#include <filesystem>
#include <fstream>
#include <iterator>

#include "brpc/controller.h"
#include "brpc/http_status_code.h"
#include "brpc/server.h"
#include "gtest/gtest.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/source/factory.h"
#include "secretflow_serving/util/network.h"
#include "secretflow_serving/util/sys_util.h"

#include "secretflow_serving/config/model_config.pb.h"
#include "secretflow_serving/source/http_service.pb.h"

namespace secretflow::serving {

std::string ArrayToString(const std::vector<uint8_t>& array) {
  std::string result;
  for (auto v : array) {
    result += std::to_string(static_cast<unsigned>(v));
    result += ',';
  }
  return result;
}

class MockHttpSource : public HttpService {
 public:
  MockHttpSource(const std::vector<uint8_t>& data) : data_(data) {}
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
    if (cntl->http_request().unresolved_path() == kDefaultNormalPath) {
      SPDLOG_INFO("response datas: {}", ArrayToString(data_));
      if (cntl->response_attachment().append(data_.data(), data_.size()) != 0) {
        SPDLOG_ERROR("fail to append response attachment");
      }
      SPDLOG_INFO("response datas size: {}",
                  cntl->response_attachment().length());
    } else if (cntl->http_request().unresolved_path() == kRelRedirectPath) {
      std::string rel_path = std::string("/HttpService/") + kDefaultNormalPath;
      SPDLOG_INFO("redirect to rel normal path: {}", rel_path);
      cntl->http_response().set_status_code(brpc::HTTP_STATUS_FOUND);
      cntl->http_response().SetHeader("Location", rel_path);
    } else if (cntl->http_request().unresolved_path() == kAbsRedirectPath) {
      std::string abs_path =
          "http://" +
          std::string(butil::endpoint2str(cntl->local_side()).c_str()) +
          std::string("/HttpService/") + kDefaultNormalPath;
      SPDLOG_INFO("redirect to abs normal path: {}", abs_path);
      cntl->http_response().set_status_code(brpc::HTTP_STATUS_FOUND);
      cntl->http_response().SetHeader("Location", abs_path);
    } else {
      SPDLOG_WARN("unknown request path: {}",
                  cntl->http_request().unresolved_path());
      cntl->http_response().set_status_code(brpc::HTTP_STATUS_NOT_FOUND);
    }
  }

  std::vector<uint8_t> data_;
  inline static const char* kDefaultNormalPath = "test_model_file";
  inline static const char* kAbsRedirectPath = "abs_redirect";
  inline static const char* kRelRedirectPath = "rel_redirect";
};

void StartServerAddRequest(const std::string& url) {
  std::vector<uint8_t> data;
  for (unsigned i = 0; i != 16; ++i) {
    auto value = static_cast<uint8_t>(((i << 4) ^ (i >> 4)) & 0xff);
    data.push_back(value);
  }
  // start server
  auto source_service = std::make_unique<MockHttpSource>(data);
  brpc::ServerOptions server_options;
  brpc::Server server;
  if (server.AddService(source_service.get(), brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add  service into brpc server.");
  }
  source_service.release();
  if (server.Start("127.0.0.1:0", &server_options) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR, "fail to start server");
  }
  std::string server_addr =
      butil::endpoint2str(server.listen_address()).c_str();
  SPDLOG_INFO("start serving server at {}", server_addr);

  // sleep to wait server ready
  sleep(2);

  // make source config
  std::string model_dir = "/tmp/serving_source_test";
  ModelConfig model_config;
  model_config.set_model_id("test_model_id");
  model_config.set_base_path(model_dir);
  model_config.set_source_type(SourceType::ST_HTTP);
  model_config.set_source_path(server_addr + url);
  auto source =
      SourceFactory::GetInstance()->Create(model_config, "test_service_id");
  // fetch file from source
  auto model_path = source->PullModel();
  // check result
  std::ifstream in(model_path, std::ios::binary);
  SERVING_ENFORCE(in.is_open(), errors::ErrorCode::INVALID_ARGUMENT,
                  "{} is not generated properly.", model_path);
  std::vector<uint8_t> value;
  uint8_t tmp;
  while (in.read(reinterpret_cast<char*>(&tmp), sizeof tmp)) {
    value.push_back(tmp);
  }
  EXPECT_EQ(data.size(), value.size());

  auto data_str = ArrayToString(data);
  auto value_str = ArrayToString(value);
  EXPECT_EQ(data_str, value_str)
      << "origin:" << data_str << ", fetched:" << value_str;
  // clean up
  std::filesystem::remove_all(model_dir);
  server.Stop(0);
  server.Join();
}

TEST(HttpSourceTest, Work) {
  StartServerAddRequest("/HttpService/test_model_file");
}

TEST(HttpSourceTest, RedirectWithAbsolutePath) {
  StartServerAddRequest("/HttpService/abs_redirect");
}

TEST(HttpSourceTest, UriRelativeResolution) {
  EXPECT_EQ(UriRelativeResolution(
                "http://localhost:9531/HttpService/rel_model_file",
                "http://localhost:9666/HttpService/test_model_file"),
            "http://localhost:9666/HttpService/test_model_file");
  EXPECT_EQ(
      UriRelativeResolution("http://localhost:9531/HttpService/rel_model_file",
                            "//localhost:9666/HttpService/test_model_file"),
      "http://localhost:9666/HttpService/test_model_file");
  EXPECT_EQ(
      UriRelativeResolution("http://localhost:9531/HttpService/test_model_file",
                            "/HttpService/test_model_file"),
      "http://localhost:9531/HttpService/test_model_file");
  EXPECT_EQ(UriRelativeResolution("http://localhost:9531/HttpService/",
                                  "test_model_file"),
            "http://localhost:9531/HttpService/test_model_file");
}

TEST(HttpSourceTest, RedirectWithRelativePath) {
  StartServerAddRequest("/HttpService/rel_redirect");
}

// disable this due to gitee api limit
TEST(HttpSourceTest, DISABLED_PullGiteeZip) {
  // make source config
  std::string model_dir = "/tmp/serving_source_test";
  ModelConfig model_config;
  model_config.set_model_id("test_model_id");
  model_config.set_base_path(model_dir);
  model_config.set_source_type(SourceType::ST_HTTP);
  model_config.set_source_path(
      "https://gitee.com/secretflow/serving/repository/archive/0.3.1b0");
  auto source =
      SourceFactory::GetInstance()->Create(model_config, "test_service_id");
  // fetch file from source
  auto model_path = source->PullModel();
  // check result
  std::ifstream in(model_path, std::ios::binary);
  SERVING_ENFORCE(in.is_open(), errors::ErrorCode::INVALID_ARGUMENT,
                  "{} is not generated properly.", model_path);
  EXPECT_EQ(1460904, std::filesystem::file_size(model_path));

  EXPECT_TRUE(SysUtil::CheckSHA256(
      model_path,
      "a8525bb8b75121dab55803086b8d2ef9d11458ce4be02bd9458d91c5095a5c02"));
  // clean up
  std::filesystem::remove_all(model_dir);
}

}  // namespace secretflow::serving
