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

#include <memory>
#include <string_view>

#include "brpc/controller.h"
#include "brpc/retry_policy.h"

#include "secretflow_serving/core/singleton.h"

#include "secretflow_serving/config/retry_policy_config.pb.h"

namespace secretflow::serving {

// not thread safe
class RetryPolicyFactory : public Singleton<RetryPolicyFactory> {
 public:
  RetryPolicyFactory();

  void SetConfig(const std::string& name, const RetryPolicyConfig* config_map);

  struct Policy {
    std::shared_ptr<brpc::RetryPolicy> policy;
    int32_t max_retry_count;
  };

  // if not setted, return default policy
  Policy GetRetryPolicyInfo(const std::string& name);

  brpc::RetryPolicy* GetRetryPolicy(const std::string& name);

  int32_t GetMaxRetryCount(const std::string& name);

 private:
  std::unordered_map<std::string, Policy> retry_policies_;
};

}  // namespace secretflow::serving
