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

#include "secretflow_serving/util/utils.h"

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
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"examples/alice/glm-test.tar.gz\",\"sourceType\":\"ST_FILE\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"examples/bob/glm-test.tar.gz\",\"sourceType\":\"ST_FILE\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}}}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":0,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}",
  "oss_meta": "",
  "spi_tls_config": "{\"certificatePath\":\"abc\", \"privateKeyPath\":\"def\",\"caFilePath\":\"gkh\"}",
  "http_source_meta": ""
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
  EXPECT_EQ("http://kd-1-communication.alice.svc", alice_p.address());

  const auto& bob_p = cluster_config.parties(1);
  EXPECT_EQ("bob", bob_p.id());
  EXPECT_EQ("http://kd-1-communication.bob.svc", bob_p.address());

  auto feature_config = config_parser.feature_config();
  EXPECT_TRUE(feature_config->has_http_opts());
  EXPECT_EQ(feature_config->http_opts().endpoint(), "alice_ep");
  EXPECT_EQ(feature_config->http_opts().tls_config().certificate_path(), "abc");
  EXPECT_EQ(feature_config->http_opts().tls_config().private_key_path(), "def");
  EXPECT_EQ(feature_config->http_opts().tls_config().ca_file_path(), "gkh");

  auto server_config = config_parser.server_config();
  EXPECT_EQ(5, server_config.feature_mapping_size());
  EXPECT_EQ("0.0.0.0", server_config.host());
  EXPECT_EQ(53509, server_config.service_port());
  EXPECT_EQ(53508, server_config.communication_port());
}

TEST_F(KusciaConfigParserTest, OSSMeta) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"examples/alice/glm-test.tar.gz\",\"sourceType\":\"ST_OSS\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"examples/bob/glm-test.tar.gz\",\"sourceType\":\"ST_OSS\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}}}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":0,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}",
  "oss_meta": "{\"accessKey\":\"test_ak\", \"secretKey\":\"test_sk\", \"virtualHosted\":true, \"endpoint\":\"test_endpoint\", \"bucket\":\"test_bucket\"}",
  "spi_tls_config": "",
  "http_source_meta": ""
}
)JSON");

  KusciaConfigParser config_parser(tmpfile.fname());

  auto model_config = config_parser.model_config();
  EXPECT_EQ("glm-test-1", model_config.model_id());
  EXPECT_EQ("/tmp/alice", model_config.base_path());
  EXPECT_EQ(SourceType::ST_OSS, model_config.source_type());
  EXPECT_EQ("test_ak", model_config.oss_source_meta().access_key());
  EXPECT_EQ("test_sk", model_config.oss_source_meta().secret_key());
  EXPECT_TRUE(model_config.oss_source_meta().virtual_hosted());
  EXPECT_EQ("test_endpoint", model_config.oss_source_meta().endpoint());
  EXPECT_EQ("test_bucket", model_config.oss_source_meta().bucket());

  EXPECT_FALSE(config_parser.feature_config()->http_opts().has_tls_config());
}

TEST_F(KusciaConfigParserTest, HttpSourceMeta) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"examples/alice/glm-test.tar.gz\",\"sourceType\":\"ST_HTTP\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"examples/bob/glm-test.tar.gz\",\"sourceType\":\"ST_HTTP\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}}}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":0,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}",
  "oss_meta": "",
  "spi_tls_config": "",
  "http_source_meta": "{\"connectTimeoutMs\":60000,\"timeoutMs\":120000,\"tlsConfig\":{\"certificatePath\":\"abc\", \"privateKeyPath\":\"def\",\"caFilePath\":\"gkh\"}}"
}
)JSON");

  KusciaConfigParser config_parser(tmpfile.fname());

  auto model_config = config_parser.model_config();
  EXPECT_EQ("glm-test-1", model_config.model_id());
  EXPECT_EQ("/tmp/alice", model_config.base_path());
  EXPECT_EQ(SourceType::ST_HTTP, model_config.source_type());
  EXPECT_EQ(60000, model_config.http_source_meta().connect_timeout_ms());
  EXPECT_EQ(120000, model_config.http_source_meta().timeout_ms());
  EXPECT_TRUE(model_config.http_source_meta().has_tls_config());
  EXPECT_EQ("abc",
            model_config.http_source_meta().tls_config().certificate_path());
  EXPECT_EQ("def",
            model_config.http_source_meta().tls_config().private_key_path());
  EXPECT_EQ("gkh", model_config.http_source_meta().tls_config().ca_file_path());

  EXPECT_FALSE(config_parser.feature_config()->http_opts().has_tls_config());
}

TEST_F(KusciaConfigParserTest, DPWorks) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\",\"dpSourceMeta\":{\"dmHost\":\"127.0.0.1:8071\",\"tls_config\":{\"certificatePath\":\"kusciaapi-server.crt\",\"privateKeyPath\":\"kusciaapi-server.key\",\"caFilePath\":\"ca.crt\"}}},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}}}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":0,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}",
  "spi_tls_config": "{\"certificate_path\":\"abc\", \"private_key_path\":\"def\",\"ca_file_path\":\"gkh\"}"
}
)JSON");

  KusciaConfigParser config_parser(tmpfile.fname());
  auto model_config = config_parser.model_config();
  EXPECT_EQ("alice-1234", model_config.source_path());
  EXPECT_EQ(SourceType::ST_DP, model_config.source_type());

  const auto& dp_source_meta = model_config.dp_source_meta();
  EXPECT_EQ("127.0.0.1:8071", dp_source_meta.dm_host());

  const auto& dp_tls_config = dp_source_meta.tls_config();
  EXPECT_EQ(dp_tls_config.certificate_path(), "kusciaapi-server.crt");
  EXPECT_EQ(dp_tls_config.private_key_path(), "kusciaapi-server.key");
  EXPECT_EQ(dp_tls_config.ca_file_path(), "ca.crt");
}

TEST_F(KusciaConfigParserTest, PredictorParties) {
  // enable alice
  // alice
  {
    butil::TempFile tmpfile;
    tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\",\"dpSourceMeta\":{\"dmHost\":\"127.0.0.1:8071\",\"tls_config\":{\"certificatePath\":\"kusciaapi-server.crt\",\"privateKeyPath\":\"kusciaapi-server.key\",\"caFilePath\":\"ca.crt\"}}},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}},\"predictorParties\":[\"alice\"]}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":0,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}"
}
)JSON");

    KusciaConfigParser config_parser(tmpfile.fname());
    auto server_config = config_parser.server_config();
    EXPECT_EQ("0.0.0.0", server_config.host());
    EXPECT_EQ(53509, server_config.service_port());
    EXPECT_EQ(53508, server_config.communication_port());
  }
  // bob
  {
    butil::TempFile tmpfile;
    tmpfile.save(1 + R"JSON(
{
  "serving_id": "kd-1",
  "input_config": "{\"partyConfigs\":{\"alice\":{\"serverConfig\":{\"featureMapping\":{\"v24\":\"x24\",\"v22\":\"x22\",\"v21\":\"x21\",\"v25\":\"x25\",\"v23\":\"x23\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/alice\",\"sourceSha256\":\"3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\",\"dpSourceMeta\":{\"dmHost\":\"127.0.0.1:8071\",\"tls_config\":{\"certificatePath\":\"kusciaapi-server.crt\",\"privateKeyPath\":\"kusciaapi-server.key\",\"caFilePath\":\"ca.crt\"}}},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"alice_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}},\"bob\":{\"serverConfig\":{\"featureMapping\":{\"v6\":\"x6\",\"v7\":\"x7\",\"v8\":\"x8\",\"v9\":\"x9\",\"v10\":\"x10\"}},\"modelConfig\":{\"modelId\":\"glm-test-1\",\"basePath\":\"/tmp/bob\",\"sourceSha256\":\"330192f3a51f9498dd882478bfe08a06501e2ed4aa2543a0fb586180925eb309\",\"sourcePath\":\"alice-1234\",\"sourceType\":\"ST_DP\"},\"featureSourceConfig\":{\"httpOpts\":{\"endpoint\":\"bob_ep\"}},\"channelDesc\":{\"protocol\":\"http\"}}},\"predictorParties\":[\"alice\"]}",
  "cluster_def": "{\"parties\":[{\"name\":\"alice\",\"role\":\"\",\"services\":[{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.alice.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.alice.svc:53510\"]},{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.alice.svc:53511\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.alice.svc\"]}]},{\"name\":\"bob\",\"role\":\"\",\"services\":[{\"portName\":\"brpc-builtin\",\"endpoints\":[\"kd-1-brpc-builtin.bob.svc:53511\"]},{\"portName\":\"service\",\"endpoints\":[\"kd-1-service.bob.svc:53508\"]},{\"portName\":\"internal\",\"endpoints\":[\"kd-1-internal.bob.svc:53510\"]},{\"portName\":\"communication\",\"endpoints\":[\"kd-1-communication.bob.svc\"]}]}],\"selfPartyIdx\":1,\"selfEndpointIdx\":0}",
  "allocated_ports": "{\"ports\":[{\"name\":\"service\",\"port\":53509,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"communication\",\"port\":53508,\"scope\":\"Cluster\",\"protocol\":\"HTTP\"},{\"name\":\"internal\",\"port\":53510,\"scope\":\"Domain\",\"protocol\":\"HTTP\"},{\"name\":\"brpc-builtin\",\"port\":53511,\"scope\":\"Domain\",\"protocol\":\"HTTP\"}]}"
}
)JSON");

    KusciaConfigParser config_parser(tmpfile.fname());
    auto server_config = config_parser.server_config();
    EXPECT_EQ("0.0.0.0", server_config.host());
    EXPECT_EQ(0, server_config.service_port());
    EXPECT_EQ(53508, server_config.communication_port());
  }
}

// TODO: exception case

}  // namespace secretflow::serving::kuscia
