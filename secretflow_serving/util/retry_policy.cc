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

#include "secretflow_serving/util/retry_policy.h"

#include <unordered_set>

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/backoff_policy.h"

namespace secretflow::serving {

namespace {

const std::unordered_set<int32_t> KCustomRetryBrpcCode = {};
const std::unordered_set<int32_t> KCustomRetryHttpCode = {500, 502, 503,
                                                          504, 408, 429};

constexpr int32_t kDefaultMaxRetryCount = 3;
const char* KDefaultPolicyName = "__default__";

}  // namespace

namespace {

template <typename BackOffT>
class RetryPolicy : public brpc::RpcRetryPolicy, public BackOffT {
 public:
  template <typename BackOffConfig>
  RetryPolicy(bool retry_custom, bool retry_aggressive,
              const BackOffConfig* config)
      : BackOffT(config),
        retry_custom_(retry_custom),
        retry_aggressive_(retry_aggressive) {
    SPDLOG_INFO("Create RetryPolicy:{}", Desc());
  }

  bool DoRetry(const brpc::Controller* cntl) const override {
    if (cntl->ErrorCode() == 0) {
      return false;
    }

    if (brpc::RpcRetryPolicy::DoRetry(cntl)) {
      return true;
    }

    // The internal design of brpc does not support retries for RPC timeout
    // errors, so it is also necessary to filter out RPC timeout error codes
    // here.
    if (cntl->ErrorCode() == brpc::ERPCTIMEDOUT) {
      return false;
    }

    if (retry_aggressive_) {
      SPDLOG_INFO("retry aggressive, {}", cntl->ErrorCode());
      return true;
    }

    if (retry_custom_) {
      auto error_code = cntl->ErrorCode();
      if (KCustomRetryBrpcCode.find(error_code) != KCustomRetryBrpcCode.end()) {
        SPDLOG_INFO("retry brpc error, {}", error_code);
        return true;
      }

      if (error_code == brpc::EHTTP &&
          KCustomRetryHttpCode.find(cntl->http_response().status_code()) !=
              KCustomRetryHttpCode.end()) {
        SPDLOG_INFO("retry on http code, {}",
                    cntl->http_response().status_code());
        return true;
      }
    }

    return false;
  }

  int32_t GetBackoffTimeMs(const brpc::Controller* cntl) const override {
    return BackOffT::GetBackOffMs(cntl->retried_count());
  }

  std::string Desc() const {
    return fmt::format("backoff_time:{}", BackOffT::Desc());
  }

 private:
  bool retry_custom_ = false;
  bool retry_aggressive_ = false;
};

template <typename BackOffConfigT>
std::shared_ptr<brpc::RetryPolicy> MakeRetryPolicy(
    bool retry_custom, bool retry_aggressive, bool has_config,
    const BackOffConfigT& config) {
  const BackOffConfigT* config_ptr = has_config ? &config : nullptr;
  if constexpr (std::is_same_v<BackOffConfigT, FixedBackOffConfig>) {
    return std::make_shared<RetryPolicy<FixedBackOffPolicy>>(
        retry_custom, retry_aggressive, config_ptr);
  } else if constexpr (std::is_same_v<BackOffConfigT,
                                      ExponentialBackOffConfig>) {
    return std::make_shared<RetryPolicy<ExponentialBackOffPolicy>>(
        retry_custom, retry_aggressive, config_ptr);
  } else if constexpr (std::is_same_v<BackOffConfigT, RandomBackOffConfig>) {
    return std::make_shared<RetryPolicy<RandomBackOffPolicy>>(
        retry_custom, retry_aggressive, config_ptr);
  } else {
    SERVING_THROW(errors::ErrorCode::LOGIC_ERROR, "unsupported backoff config");
  }
}

}  // namespace

RetryPolicyFactory::RetryPolicyFactory() {
  std::shared_ptr<brpc::RetryPolicy> policy =
      std::make_shared<RetryPolicy<FixedBackOffPolicy>>(
          false, false, static_cast<FixedBackOffConfig*>(nullptr));
  retry_policies_.emplace(KDefaultPolicyName,
                          Policy{policy, kDefaultMaxRetryCount});
}

void RetryPolicyFactory::SetConfig(const std::string& name,
                                   const RetryPolicyConfig* config) {
  std::shared_ptr<brpc::RetryPolicy> policy =
      std::make_shared<RetryPolicy<FixedBackOffPolicy>>(
          false, false, static_cast<FixedBackOffConfig*>(nullptr));

  if (retry_policies_.find(name) != retry_policies_.end()) {
    SPDLOG_WARN("{} already exists, overwrite it", name);
  }

  auto custom_retry = false;
  auto retry_aggressive = false;
  int32_t max_retry_count = kDefaultMaxRetryCount;

  if (config != nullptr) {
    custom_retry = config->retry_custom();
    retry_aggressive = config->retry_aggressive();

    if (config->max_retry_count() != 0) {
      max_retry_count = config->max_retry_count();
    }

    switch (config->backoff_mode()) {
      case RetryPolicyBackOffMode::EXPONENTIAL_BACKOFF:
        policy = MakeRetryPolicy(custom_retry, retry_aggressive,
                                 config->has_exponential_backoff_config(),
                                 config->exponential_backoff_config());
        break;
      case RetryPolicyBackOffMode::RANDOM_BACKOFF:
        policy = MakeRetryPolicy(custom_retry, retry_aggressive,
                                 config->has_random_backoff_config(),
                                 config->random_backoff_config());
        break;
      case RetryPolicyBackOffMode::FIXED_BACKOFF:
      // full_through
      default:
        policy = MakeRetryPolicy(custom_retry, retry_aggressive,
                                 config->has_fixed_backoff_config(),
                                 config->fixed_backoff_config());
        break;
    }
  }
  SPDLOG_INFO("Regist retry policy: name={}", name);
  retry_policies_[name] = Policy{policy, max_retry_count};
}

RetryPolicyFactory::Policy RetryPolicyFactory::GetRetryPolicyInfo(
    const std::string& name) {
  auto iter = retry_policies_.find(name);
  if (iter == retry_policies_.end()) {
    SPDLOG_WARN("{} not found, use default policy instead", name);
    iter = retry_policies_.find(KDefaultPolicyName);
    SERVING_ENFORCE(iter != retry_policies_.end(),
                    errors::ErrorCode::LOGIC_ERROR,
                    "retry policy factory has not a default policy");
  }

  return iter->second;
}

int32_t RetryPolicyFactory::GetMaxRetryCount(const std::string& name) {
  auto policy = GetRetryPolicyInfo(name);
  return policy.max_retry_count;
}

brpc::RetryPolicy* RetryPolicyFactory::GetRetryPolicy(const std::string& name) {
  auto policy = GetRetryPolicyInfo(name);
  return policy.policy.get();
}

}  // namespace secretflow::serving
