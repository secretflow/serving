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

#pragma once

#include <string>

#include "brpc/channel.h"

#include "secretflow_serving/source/source.h"

namespace brpc {
class Channel;
}

namespace secretflow::serving {

std::string UriRelativeResolution(const std::string& origin_path,
                                  const std::string& redirect_path);

class HttpSource : public Source {
 public:
  HttpSource(const ModelConfig& config, const std::string& service_id);
  ~HttpSource() = default;

 protected:
  void OnPullModel(const std::string& dst_path) override;

 protected:
  std::string endpoint_;
  inline static constexpr size_t kHttpTimeoutMs = 120 * 1000;
  inline static constexpr size_t kConnectTimeoutMs = 60 * 1000;
  size_t http_timeout_ms_ = kHttpTimeoutMs;
  size_t connect_timeout_ms_ = kConnectTimeoutMs;
  std::unique_ptr<TlsConfig> tls_config_;
};

}  // namespace secretflow::serving
