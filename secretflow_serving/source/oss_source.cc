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

#include "secretflow_serving/source/oss_source.h"

#include "secretflow_serving/source/factory.h"

namespace secretflow::serving {

OssSource::OssSource(const ModelConfig& config, const std::string& service_id)
    : Source(config, service_id) {
  SERVING_ENFORCE(config.has_oss_source_meta(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "failed to find oss source meta");

  OssOptions oss_opts;
  oss_opts.virtual_hosted = config.oss_source_meta().virtual_hosted();
  oss_opts.access_key_id = config.oss_source_meta().access_key();
  oss_opts.secret_key = config.oss_source_meta().secret_key();
  oss_opts.endpoint = config.oss_source_meta().endpoint();
  oss_opts.bucket = config.oss_source_meta().bucket();
  oss_client_ = std::make_unique<OssClient>(oss_opts);
}

void OssSource::OnPullModel(const std::string& dst_path) {
  oss_client_->GetFile(config_.source_path(), dst_path);
}

REGISTER_SOURCE(SourceType::ST_OSS, OssSource);

}  // namespace secretflow::serving
