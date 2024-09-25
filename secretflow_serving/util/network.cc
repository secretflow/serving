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

#include "secretflow_serving/util/network.h"

#include "absl/strings/match.h"

#include "secretflow_serving/util/retry_policy.h"

namespace secretflow::serving {

namespace {

const std::string kHttpPrefix = "http://";
const std::string kHttpsPrefix = "https://";
const std::string kDefaultLoadBalancer = "rr";

const int32_t kPeerConnectTimeoutMs = 500;
const int32_t kPeerRpcTimeoutMs = 2000;

std::string FillHttpPrefix(const std::string& addr, bool ssl_enabled) {
  if (absl::StartsWith(addr, kHttpPrefix) ||
      absl::StartsWith(addr, kHttpsPrefix)) {
    return addr;
  }
  if (ssl_enabled) {
    return kHttpsPrefix + addr;
  } else {
    return kHttpPrefix + addr;
  }
}

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& endpoint, bool enable_lb,
    const brpc::ChannelOptions& opts) {
  auto channel = std::make_unique<brpc::Channel>();
  std::string remote_url = endpoint;
  std::string load_balancer;
  if (enable_lb) {
    // 使用负责均衡策略时，需要保证url以协议开头
    remote_url = FillHttpPrefix(endpoint, opts.has_ssl_options());
    load_balancer = kDefaultLoadBalancer;
  }

  int res = channel->Init(remote_url.c_str(), load_balancer.c_str(), &opts);
  SERVING_ENFORCE(
      res == 0, errors::ErrorCode::UNEXPECTED_ERROR,
      "failed to init brpc channel to host={}, lb={}, error code={}",
      remote_url, load_balancer, res);
  return channel;
}

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& endpoint, const std::string& protocol, bool enable_lb,
    int32_t rpc_timeout_ms, int32_t connect_timeout_ms,
    const TlsConfig* tls_config, brpc::ChannelOptions& opts) {
  opts.protocol = protocol.empty() ? "baidu_std" : protocol;
  if (rpc_timeout_ms > 0) {
    opts.timeout_ms = rpc_timeout_ms;
  }
  if (connect_timeout_ms > 0) {
    opts.connect_timeout_ms = connect_timeout_ms;
  }
  if (tls_config != nullptr) {
    opts.mutable_ssl_options()->client_cert.certificate =
        tls_config->certificate_path();
    opts.mutable_ssl_options()->client_cert.private_key =
        tls_config->private_key_path();
    if (!tls_config->ca_file_path().empty()) {
      opts.mutable_ssl_options()->verify.ca_file_path =
          tls_config->ca_file_path();
      // use default verify depth
      opts.mutable_ssl_options()->verify.verify_depth = 1;
    }
  }
  return CreateBrpcChannel(endpoint, enable_lb, opts);
}

}  // namespace

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& endpoint, const std::string& protocol, bool enable_lb,
    int32_t rpc_timeout_ms, int32_t connect_timeout_ms,
    const TlsConfig* tls_config) {
  brpc::ChannelOptions opts;
  return CreateBrpcChannel(endpoint, protocol, enable_lb, rpc_timeout_ms,
                           connect_timeout_ms, tls_config, opts);
}

std::unique_ptr<google::protobuf::RpcChannel> CreateBrpcChannel(
    const std::string& name, const std::string& endpoint,
    const std::string& protocol, bool enable_lb, int32_t rpc_timeout_ms,
    int32_t connect_timeout_ms, const TlsConfig* tls_config,
    const RetryPolicyConfig* retry_policy_config) {
  brpc::ChannelOptions opts;

  RetryPolicyFactory::GetInstance()->SetConfig(name, retry_policy_config);
  opts.retry_policy = RetryPolicyFactory::GetInstance()->GetRetryPolicy(name);
  opts.max_retry = RetryPolicyFactory::GetInstance()->GetMaxRetryCount(name);

  return CreateBrpcChannel(endpoint, protocol, enable_lb, rpc_timeout_ms,
                           connect_timeout_ms, tls_config, opts);
}

std::shared_ptr<
    std::map<std::string, std::unique_ptr<::google::protobuf::RpcChannel>>>
BuildChannelsFromConfig(const ClusterConfig& cluster_config,
                        bool enable_peers_load_balancer) {
  SERVING_ENFORCE(cluster_config.parties_size() > 1,
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "too few parties params for cluster config, get: {}",
                  cluster_config.parties_size());

  auto channels = std::make_shared<
      std::map<std::string, std::unique_ptr<::google::protobuf::RpcChannel>>>();
  for (const auto& party : cluster_config.parties()) {
    if (party.id() == cluster_config.self_id()) {
      continue;
    }
    const auto& channel_desc = cluster_config.channel_desc();
    channels->emplace(
        party.id(),
        CreateBrpcChannel(
            party.id(), party.address(), channel_desc.protocol(),
            enable_peers_load_balancer,
            channel_desc.rpc_timeout_ms() > 0 ? channel_desc.rpc_timeout_ms()
                                              : kPeerRpcTimeoutMs,
            channel_desc.connect_timeout_ms() > 0
                ? channel_desc.connect_timeout_ms()
                : kPeerConnectTimeoutMs,
            channel_desc.has_tls_config() ? &channel_desc.tls_config()
                                          : nullptr,
            channel_desc.has_retry_policy_config()
                ? &channel_desc.retry_policy_config()
                : nullptr));
  }

  return channels;
}

}  // namespace secretflow::serving
