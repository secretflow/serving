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

#include <fstream>
#include <streambuf>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/utils.h"

#include "kuscia/proto/api/v1alpha1/appconfig/app_config.pb.h"
#include "secretflow_serving/server/kuscia/serving_config.pb.h"

namespace secretflow::serving::kuscia {

namespace {
const char* kSpiCertEnv = "SERVING_SPI_CERT";
const char* kSpiPrivateKeyEnv = "SERVING_SPI_PRIVATE_KEY";
const char* kSpiCaEnv = "SERVING_SPI_CA";

void DumpFile(const std::string& file_path, const std::string& content) {
  std::ofstream outfile(file_path);
  SERVING_ENFORCE(outfile.is_open(), errors::ErrorCode::IO_ERROR,
                  "cat not open file:{} to dump content.", file_path);
  outfile << content;
  outfile.close();
}

}  // namespace

namespace kusica_proto = ::kuscia::proto::api::v1alpha1::appconfig;

KusciaConfigParser::KusciaConfigParser(const std::string& config_file) {
  std::ifstream file_is(config_file);
  std::string raw_config_str((std::istreambuf_iterator<char>(file_is)),
                             std::istreambuf_iterator<char>());

  SPDLOG_INFO("raw kuscia serving config content: {}", raw_config_str);

  rapidjson::Document doc;
  SERVING_ENFORCE(!doc.Parse(raw_config_str.c_str()).HasParseError(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "Failed to parse config str to json, error=(offset={}, "
                  "code={}), raw str: {}",
                  doc.GetErrorOffset(), static_cast<int>(doc.GetParseError()),
                  raw_config_str);

  // get services id
  SERVING_ENFORCE(doc["serving_id"].IsString(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  service_id_ = {doc["serving_id"].GetString(),
                 doc["serving_id"].GetStringLength()};

  int self_party_idx = 0;
  std::string self_party_id;
  {
    // parse cluster_def
    SERVING_ENFORCE(doc["cluster_def"].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string cluster_def_str = {doc["cluster_def"].GetString(),
                                   doc["cluster_def"].GetStringLength()};

    kusica_proto::ClusterDefine cluster_def;
    JsonToPb(cluster_def_str, &cluster_def);
    self_party_idx = cluster_def.self_party_idx();

    for (int i = 0; i < cluster_def.parties_size(); ++i) {
      const auto& p = cluster_def.parties(i);
      if (i == cluster_def.self_party_idx()) {
        cluster_config_.set_self_id(p.name());
        self_party_id = p.name();
      }
      auto party = cluster_config_.add_parties();
      party->set_id(p.name());
      for (const auto& s : p.services()) {
        if (s.port_name() == "service") {
          // add "http://" to force brpc to set the correct Host
          party->set_address(fmt::format("http://{}", s.endpoints(0)));
        }
      }
    }

    SERVING_ENFORCE_GT(cluster_config_.parties_size(), 1,
                       "too few cluster party config to run serving.");
  }

  {
    // parse input config
    SERVING_ENFORCE(doc["input_config"].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string input_config_str = {doc["input_config"].GetString(),
                                    doc["input_config"].GetStringLength()};

    KusciaServingConfig serving_config;
    JsonToPb(input_config_str, &serving_config);
    auto iter = serving_config.party_configs().find(self_party_id);
    SERVING_ENFORCE(iter != serving_config.party_configs().end(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    const auto& party_config = iter->second;
    server_config_ = party_config.server_config();
    model_config_ = party_config.model_config();
    if (party_config.has_feature_source_config()) {
      feature_config_ = party_config.feature_source_config();
    }
    *cluster_config_.mutable_channel_desc() = party_config.channel_desc();
  }

  {
    // parse allocated_ports
    SERVING_ENFORCE(doc["allocated_ports"].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string allocated_ports_str = {
        doc["allocated_ports"].GetString(),
        doc["allocated_ports"].GetStringLength()};

    kusica_proto::AllocatedPorts allocated_ports;
    JsonToPb(allocated_ports_str, &allocated_ports);
    for (const auto& p : allocated_ports.ports()) {
      if (p.name() == "service") {
        cluster_config_.mutable_parties(self_party_idx)
            ->set_listen_address(fmt::format("0.0.0.0:{}", p.port()));
      }
      if (p.name() == "brpc-builtin") {
        server_config_.set_brpc_builtin_service_port(p.port());
      }
      if (p.name() == "internal") {
        server_config_.set_metrics_exposer_port(p.port());
      }
    }
  }

  // load oss config
  if (model_config_.source_type() == SourceType::ST_OSS) {
    SERVING_ENFORCE(doc["oss_meta"].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string oss_meta_str = {doc["oss_meta"].GetString(),
                                doc["oss_meta"].GetStringLength()};
    OSSSourceMeta oss_meta;
    JsonToPb(oss_meta_str, model_config_.mutable_oss_source_meta());
  }

  // fill spi tls config
  if (feature_config_.has_value() && feature_config_->has_http_opts()) {
    auto* http_opts = feature_config_->mutable_http_opts();
    if (char* env_p = std::getenv(kSpiCertEnv)) {
      if (strlen(env_p) != 0) {
        std::string file_path = "./serving_spi_cert";
        DumpFile(file_path, env_p);
        http_opts->mutable_tls_config()->set_certificate_path(file_path);
      }
    }
    if (char* env_p = std::getenv(kSpiPrivateKeyEnv)) {
      if (strlen(env_p) != 0) {
        std::string file_path = "./serving_spi_pk";
        DumpFile(file_path, env_p);
        http_opts->mutable_tls_config()->set_private_key_path(file_path);
      }
    }
    if (char* env_p = std::getenv(kSpiCaEnv)) {
      if (strlen(env_p) != 0) {
        std::string file_path = "./serving_spi_ca";
        DumpFile(file_path, env_p);
        http_opts->mutable_tls_config()->set_ca_file_path(file_path);
      }
    }
  }
}

}  // namespace secretflow::serving::kuscia
