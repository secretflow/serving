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
#include <string>

#include "brpc/channel.h"

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/config/retry_policy_config.pb.h"
#include "secretflow_serving/config/tls_config.pb.h"

namespace secretflow::serving {

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& name, const std::string& endpoint,
    const std::string& protocol, bool enable_lb, int32_t rpc_timeout_ms,
    int32_t connect_timeout_ms, const TlsConfig* tls_config,
    const RetryPolicyConfig* retry_policy_config = nullptr);

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& endpoint, const std::string& protocol, bool enable_lb,
    int32_t rpc_timeout_ms, int32_t connect_timeout_ms,
    const TlsConfig* tls_config);

}  // namespace secretflow::serving
