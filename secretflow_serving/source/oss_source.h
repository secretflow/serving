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

#pragma once

#include <memory>

#include "secretflow_serving/source/source.h"
#include "secretflow_serving/util/oss_client.h"

#include "secretflow_serving/config/model_config.pb.h"

namespace secretflow::serving {

class OssSource : public Source {
 public:
  OssSource(const ModelConfig& config, const std::string& service_id);
  ~OssSource() = default;

 protected:
  virtual void OnPullModel(const std::string& dst_path) override;

 private:
  std::unique_ptr<OssClient> oss_client_;
  OssOptions oss_opts_;
};

}  // namespace secretflow::serving
