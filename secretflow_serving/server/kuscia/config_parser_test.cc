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

#include "secretflow_serving/server/kuscia/config_parser.h"

#include "butil/files/temp_file.h"
#include "gtest/gtest.h"

namespace secretflow::serving::kuscia {

class KusciaConfigParserTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(KusciaConfigParserTest, Works) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"examples/alice/glm-test.tar.gz\",\"sourceType\":\"ST_FILE\"},\"featureSourceConfig\":{\"mockOpts\":{}},\"channel_desc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"examples/bob/glm-test.tar.gz\",\"sourceType\":\"ST_FILE\"},\"featureSourceConfig\":{\"mockOpts\":{}},\"channel_desc\":{\"protocol\":\"http\"}}}}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\", \"role\":\"\", \"services\":[{\"portName\":\"service\", \"endpoints\":[\"kd-1-service.alice.svc\"]}, {\"portName\":\"internal\", \"endpoints\":[\"kd-1-internal.alice.svc:53510\"]}, {\"portName\":\"brpc-builtin\", \"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]}]}, {\"name\":\"bob\", \"role\":\"\", \"services\":[{\"portName\":\"brpc-builtin\", \"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]}, {\"portName\":\"service\", \"endpoints\":[\"kd-1-service.bob.svc\"]}, {\"portName\":\"internal\", \"endpoints\":[\"kd-1-internal.bob.svc:53510\"]}]}], \"selfPartyIdx\":0, \"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\", \"port\":53509, \"scope\":\"Cluster\", \"protocol\":\"HTTP\"}, {\"name\":\"internal\", \"port\":53510, \"scope\":\"Domain\", \"protocol\":\"HTTP\"}, {\"name\":\"brpc-builtin\", \"port\":53511, \"scope\":\"Domain\", \"protocol\":\"HTTP\"}]}",
  "oss_meta": ""
}
)JSON");

  KusciaConfigParser config_parser(tmpfile.fname());

  EXPECT_EQ("kd-1", config_parser.service_id());

  auto model_config = config_parser.model_config();
  EXPECT_EQ("glm-test-1", model_config.model_id());
  EXPECT_EQ("/tmp/alice", model_config.base_path());
  EXPECT_EQ(SourceType::ST_FILE, model_config.source_type());
  EXPECT_EQ("3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522",
            model_config.source_sha256());

  auto cluster_config = config_parser.cluster_config();
  EXPECT_EQ(2, cluster_config.parties_size());
  EXPECT_EQ("alice", cluster_config.self_id());
  EXPECT_EQ("http", cluster_config.channel_desc().protocol());

  const auto& alice_p = cluster_config.parties(0);
  EXPECT_EQ("alice", alice_p.id());
  EXPECT_EQ("http://kd-1-service.alice.svc", alice_p.address());
  EXPECT_EQ("0.0.0.0:53509", alice_p.listen_address());

  const auto& bob_p = cluster_config.parties(1);
  EXPECT_EQ("bob", bob_p.id());
  EXPECT_EQ("http://kd-1-service.bob.svc", bob_p.address());
  EXPECT_TRUE(bob_p.listen_address().empty());

  auto feature_config = config_parser.feature_config();
  EXPECT_TRUE(feature_config->has_mock_opts());

  auto server_config = config_parser.server_config();
  EXPECT_EQ(5, server_config.feature_mapping_size());
}

// TODO: exception case

}  // namespace secretflow::serving::kuscia
