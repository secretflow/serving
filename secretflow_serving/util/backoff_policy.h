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

#include <cmath>
#include <random>
#include <string>

#include "spdlog/spdlog.h"

#include "secretflow_serving/config/retry_policy_config.pb.h"

namespace secretflow::serving {

class FixedBackOffPolicy {
 public:
  explicit FixedBackOffPolicy(const FixedBackOffConfig* config = nullptr) {
    if (config && config->interval_ms()) {
      interval_ms_ = config->interval_ms();
    }
  }
  int32_t GetBackOffMs(uint32_t retry_count) const { return interval_ms_; }

  std::string Desc() const { return fmt::format("{}ms", interval_ms_); }

 private:
  int32_t interval_ms_ = 10;
};

class ExponentialBackOffPolicy {
 public:
  explicit ExponentialBackOffPolicy(
      const ExponentialBackOffConfig* config = nullptr) {
    if (config && config->init_ms()) {
      init_ms_ = config->init_ms();
    }
    if (config && config->factor()) {
      factor_ = config->factor();
    }
  }
  int32_t GetBackOffMs(uint32_t retry_count) const {
    return init_ms_ * std::pow(factor_, retry_count);
  }

  std::string Desc() const {
    return fmt::format("{}*{}^retry_cnt ms", init_ms_, factor_);
  }

 private:
  int32_t init_ms_ = 10;
  int32_t factor_ = 2;
};

class RandomBackOffPolicy {
 public:
  explicit RandomBackOffPolicy(const RandomBackOffConfig* config = nullptr) {
    if (config && config->min_ms()) {
      min_ms_ = config->min_ms();
    }
    if (config && config->max_ms()) {
      max_ms_ = config->max_ms();
    }
  }
  int32_t GetBackOffMs(uint32_t /* retry_count */) const {
    std::random_device device;
    std::mt19937 gen(device());
    std::uniform_int_distribution<> uniform_dist(min_ms_, max_ms_);
    return uniform_dist(gen);
  }

  std::string Desc() const { return fmt::format("[{}, {}]", min_ms_, max_ms_); }

 private:
  int32_t min_ms_ = 10;
  int32_t max_ms_ = 50;
};

}  // namespace secretflow::serving
