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

#include "secretflow_serving/source/dataproxy_source.h"

#include "dataproxy_sdk/cc/api.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/source/factory.h"

namespace secretflow::serving {

DataProxySource::DataProxySource(const ModelConfig& config,
                                 const std::string& service_id)
    : Source(config, service_id) {}

void DataProxySource::OnPullModel(const std::string& dst_path) {
  dataproxy_sdk::proto::DataProxyConfig sdk_config;
  sdk_config.set_data_proxy_addr(config_.dp_source_meta().dm_host());

  if (config_.dp_source_meta().has_tls_config()) {
    sdk_config.mutable_tls_config()->set_certificate_path(
        config_.dp_source_meta().tls_config().certificate_path());
    sdk_config.mutable_tls_config()->set_private_key_path(
        config_.dp_source_meta().tls_config().private_key_path());
    sdk_config.mutable_tls_config()->set_ca_file_path(
        config_.dp_source_meta().tls_config().ca_file_path());
  }

  auto dp_file = dataproxy_sdk::DataProxyFile::Make(sdk_config);

  dataproxy_sdk::proto::DownloadInfo download_info;
  download_info.set_domaindata_id(config_.source_path());
  dp_file->DownloadFile(download_info, dst_path,
                        dataproxy_sdk::proto::FileFormat::BINARY);
  dp_file->Close();
  SPDLOG_INFO(
      "DataProxySource:download model file from domain id {} to {} success.",
      config_.source_path(), dst_path);
}

REGISTER_SOURCE(SourceType::ST_DP, DataProxySource);

}  // namespace secretflow::serving
