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

#include "gtest/gtest.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/source/factory.h"

#include "secretflow_serving/config/model_config.pb.h"

namespace secretflow::serving {

TEST(SourceTest, FileSystemSourceTest) {
  // make source config
  std::string model_dir = "/tmp/serving_source_test";
  ModelConfig model_config;
  model_config.set_model_id("test_model_id");
  model_config.set_base_path(model_dir + "_dest");
  model_config.set_source_type(SourceType::ST_FILE);
  model_config.set_source_path(model_dir);
  EXPECT_NO_THROW(
      SourceFactory::GetInstance()->Create(model_config, "test_service_id"));
}

TEST(SourceTest, OssSourceTest) {
  // make source config
  std::string model_dir = "/tmp/serving_source_test";
  ModelConfig model_config;
  model_config.set_model_id("test_model_id");
  model_config.set_base_path(model_dir);
  model_config.set_source_type(SourceType::ST_OSS);
  model_config.set_source_path("https://somewhere.com/");
  model_config.mutable_oss_source_meta()->set_access_key("some_key1");
  model_config.mutable_oss_source_meta()->set_secret_key("some_key2");
  EXPECT_NO_THROW(
      SourceFactory::GetInstance()->Create(model_config, "test_service_id"));
}

TEST(SourceTest, HttpSourceTest) {
  // make source config
  std::string model_dir = "/tmp/serving_source_test";
  ModelConfig model_config;
  model_config.set_model_id("test_model_id");
  model_config.set_base_path(model_dir);
  model_config.set_source_type(SourceType::ST_HTTP);
  model_config.set_source_path("https://somewhere.com/some_file.tar.gz");
  EXPECT_NO_THROW(
      SourceFactory::GetInstance()->Create(model_config, "test_service_id"));
}

}  // namespace secretflow::serving
