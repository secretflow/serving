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
const char* kSpiTlsConfigKey = "spi_tls_config";

const char* kServingIdKey = "serving_id";

const char* kClusterDefKey = "cluster_def";

const char* kInputConfigKey = "input_config";

const char* kAllocatedPortKey = "allocated_ports";

const char* kOssMetaKey = "oss_meta";

const char* kHttpSourceMetaKey = "http_source_meta";
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
  SERVING_ENFORCE(doc[kServingIdKey].IsString(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  service_id_ = {doc[kServingIdKey].GetString(),
                 doc[kServingIdKey].GetStringLength()};

  int self_party_idx = 0;
  std::string self_party_id;
  {
    // parse cluster_def
    SERVING_ENFORCE(doc[kClusterDefKey].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string cluster_def_str = {doc[kClusterDefKey].GetString(),
                                   doc[kClusterDefKey].GetStringLength()};

    kusica_proto::ClusterDefine cluster_def;
    JsonToPb(cluster_def_str, &cluster_def);
    self_party_idx = cluster_def.self_party_idx();

    for (int i = 0; i < cluster_def.parties_size(); ++i) {
      const auto& p = cluster_def.parties(i);
      if (i == self_party_idx) {
        cluster_config_.set_self_id(p.name());
        self_party_id = p.name();
      }
      auto* party = cluster_config_.add_parties();
      party->set_id(p.name());
      for (const auto& s : p.services()) {
        if (s.port_name() == "communication") {
          // add "http://" to force brpc to set the correct Host
          party->set_address(fmt::format("http://{}", s.endpoints(0)));
        }
      }
      SERVING_ENFORCE(!party->address().empty(),
                      errors::ErrorCode::INVALID_ARGUMENT,
                      "party {} communication endpoint is empty.", party->id());
    }

    SERVING_ENFORCE_GT(cluster_config_.parties_size(), 1,
                       "too few cluster party config to run serving.");
  }

  // parse input config
  SERVING_ENFORCE(doc[kInputConfigKey].IsString(),
                  errors::ErrorCode::INVALID_ARGUMENT);
  std::string input_config_str = {doc[kInputConfigKey].GetString(),
                                  doc[kInputConfigKey].GetStringLength()};

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

  // determine whether to listen for predictive services
  bool enable_pred_svc =
      serving_config.predictor_parties().empty() ? true : false;
  for (const auto& p_id : serving_config.predictor_parties()) {
    bool flag = false;
    for (const auto& p_desc : cluster_config_.parties()) {
      if (p_desc.id() == p_id) {
        flag = true;
        break;
      }
    }
    SERVING_ENFORCE(flag, errors::ErrorCode::INVALID_ARGUMENT,
                    "invalid predictor party: {}", p_id);

    if (p_id == self_party_id) {
      enable_pred_svc = true;
    }
  }

  {
    // parse allocated_ports
    SERVING_ENFORCE(doc[kAllocatedPortKey].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string allocated_ports_str = {
        doc[kAllocatedPortKey].GetString(),
        doc[kAllocatedPortKey].GetStringLength()};

    kusica_proto::AllocatedPorts allocated_ports;
    JsonToPb(allocated_ports_str, &allocated_ports);
    server_config_.set_host("0.0.0.0");
    for (const auto& p : allocated_ports.ports()) {
      if (enable_pred_svc && p.name() == "service") {
        server_config_.set_service_port(p.port());
      }
      if (p.name() == "communication") {
        server_config_.set_communication_port(p.port());
      }
      if (p.name() == "brpc-builtin") {
        server_config_.set_brpc_builtin_service_port(p.port());
      }
      if (p.name() == "internal") {
        server_config_.set_metrics_exposer_port(p.port());
      }
    }

    SERVING_ENFORCE_NE(server_config_.communication_port(), 0,
                       "get empty communication port.");
    if (enable_pred_svc) {
      SERVING_ENFORCE_NE(server_config_.service_port(), 0,
                         "get empty service port.");
    }
  }

  // load oss config
  if (model_config_.source_type() == SourceType::ST_OSS) {
    SERVING_ENFORCE(doc.HasMember(kOssMetaKey),
                    errors::ErrorCode::INVALID_ARGUMENT);
    SERVING_ENFORCE(doc[kOssMetaKey].IsString(),
                    errors::ErrorCode::INVALID_ARGUMENT);
    std::string oss_meta_str = {doc[kOssMetaKey].GetString(),
                                doc[kOssMetaKey].GetStringLength()};
    SERVING_ENFORCE(!oss_meta_str.empty(), errors::ErrorCode::INVALID_ARGUMENT,
                    "get empty `oss_meta`");
    JsonToPb(oss_meta_str, model_config_.mutable_oss_source_meta());
  } else if (model_config_.source_type() == SourceType::ST_HTTP) {
    if (doc.HasMember(kHttpSourceMetaKey)) {
      SERVING_ENFORCE(doc[kHttpSourceMetaKey].IsString(),
                      errors::ErrorCode::INVALID_ARGUMENT);
      std::string meta_str = {doc[kHttpSourceMetaKey].GetString(),
                              doc[kHttpSourceMetaKey].GetStringLength()};
      if (!meta_str.empty()) {
        JsonToPb(meta_str, model_config_.mutable_http_source_meta());
      }
    }
  }

  // kuscia data proxy source
  // First get from the env, if no configuration is used
  if (model_config_.source_type() == SourceType::ST_DP) {
    // try to get tls config from kuscia envs
    DPSourceMeta* dp_meta = model_config_.mutable_dp_source_meta();
    if (char* env_p = std::getenv("CLIENT_CERT_FILE")) {
      if (strlen(env_p) != 0) {
        dp_meta->mutable_tls_config()->set_certificate_path(env_p);
      }
    }
    if (char* env_p = std::getenv("CLIENT_PRIVATE_KEY_FILE")) {
      if (strlen(env_p) != 0) {
        dp_meta->mutable_tls_config()->set_private_key_path(env_p);
      }
    }
    if (char* env_p = std::getenv("TRUSTED_CA_FILE")) {
      if (strlen(env_p) != 0) {
        dp_meta->mutable_tls_config()->set_ca_file_path(env_p);
      }
    }
    if (char* dm_addr = std::getenv("KUSCIA_DATA_MESH_ADDR")) {
      if (strlen(dm_addr) != 0) {
        dp_meta->set_dm_host(dm_addr);
      }
    }
    if (dp_meta->dm_host().empty()) {
      SPDLOG_WARN("use default datamesh address: datamesh:8071");
      dp_meta->set_dm_host("datamesh:8071");
    }
  }

  // fill spi tls config
  if (feature_config_.has_value() && feature_config_->has_http_opts()) {
    auto* http_opts = feature_config_->mutable_http_opts();
    if (doc.HasMember(kSpiTlsConfigKey)) {
      SERVING_ENFORCE(doc[kSpiTlsConfigKey].IsString(),
                      errors::ErrorCode::INVALID_ARGUMENT);
      std::string spi_tls_config_str = {
          doc[kSpiTlsConfigKey].GetString(),
          doc[kSpiTlsConfigKey].GetStringLength()};
      if (!spi_tls_config_str.empty()) {
        SPDLOG_INFO("spi tls config: {}", spi_tls_config_str);
        JsonToPb(spi_tls_config_str, http_opts->mutable_tls_config());
      }
    }
  }
}

}  // namespace secretflow::serving::kuscia
