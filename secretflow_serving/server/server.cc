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

#include "secretflow_serving/server/server.h"

#include "absl/strings/str_split.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/framework/model_info_collector.h"
#include "secretflow_serving/framework/model_loader.h"
#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/server/execution_service_impl.h"
#include "secretflow_serving/server/health.h"
#include "secretflow_serving/server/metrics/default_metrics_registry.h"
#include "secretflow_serving/server/metrics/metrics_service.h"
#include "secretflow_serving/server/prediction_service_impl.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/source/factory.h"
#include "secretflow_serving/util/network.h"
#include "secretflow_serving/util/retry_policy.h"

#include "secretflow_serving/apis/execution_service.pb.h"
#include "secretflow_serving/apis/metrics.pb.h"
#include "secretflow_serving/apis/prediction_service.pb.h"

DEFINE_bool(enable_peers_load_balancer, false,
            "whether to enable load balancer between parties");

namespace secretflow::serving {

namespace {

const int32_t kPeerConnectTimeoutMs = 500;
const int32_t kPeerRpcTimeoutMs = 2000;

void SetServerTLSOpts(const TlsConfig& tls_config,
                      brpc::ServerSSLOptions* server_ssl_opts) {
  server_ssl_opts->default_cert.certificate = tls_config.certificate_path();
  server_ssl_opts->default_cert.private_key = tls_config.private_key_path();
  if (!tls_config.ca_file_path().empty()) {
    server_ssl_opts->verify.verify_depth = 1;
    server_ssl_opts->verify.ca_file_path = tls_config.ca_file_path();
  }
}

}  // namespace

Server::Server(Options opts) : opts_(std::move(opts)) {
  SERVING_ENFORCE(opts_.cluster_config.parties_size() > 1,
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "too few parties params for cluster config, get: {}",
                  opts_.cluster_config.parties_size());
}

Server::~Server() {
  service_server_.Stop(0);
  communication_server_.Stop(0);
  metrics_server_.Stop(0);
  service_server_.Join();
  communication_server_.Join();
  metrics_server_.Join();

  model_service_ = nullptr;
}

void Server::Start() {
  const auto& self_party_id = opts_.cluster_config.self_id();

  // get model package
  auto source = SourceFactory::GetInstance()->Create(opts_.model_config,
                                                     opts_.service_id);
  auto package_path = source->PullModel();

  std::string host = opts_.server_config.host();
  SERVING_ENFORCE(!host.empty(), errors::ErrorCode::INVALID_ARGUMENT,
                  "get empty host.");

  // build channels
  std::vector<std::string> cluster_ids;
  auto channels = std::make_shared<PartyChannelMap>();
  for (const auto& party : opts_.cluster_config.parties()) {
    cluster_ids.emplace_back(party.id());
    if (party.id() == self_party_id) {
      continue;
    }
    const auto& channel_desc = opts_.cluster_config.channel_desc();
    channels->emplace(
        party.id(),
        CreateBrpcChannel(
            party.id(), party.address(), channel_desc.protocol(),
            FLAGS_enable_peers_load_balancer,
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

  auto com_address =
      fmt::format("{}:{}", host, opts_.server_config.communication_port());
  auto service_address =
      fmt::format("{}:{}", host, opts_.server_config.service_port());

  // load model package
  auto loader = std::make_unique<ModelLoader>();
  loader->Load(package_path);
  const auto& model_bundle = loader->GetModelBundle();
  Graph graph(model_bundle->graph());

  // build execution core
  std::vector<Executor> executors;
  for (const auto& execution : graph.GetExecutions()) {
    executors.emplace_back(Executor(execution));
  }
  ExecutionCore::Options exec_opts;
  exec_opts.id = opts_.service_id;
  exec_opts.party_id = self_party_id;
  exec_opts.executable = std::make_unique<Executable>(std::move(executors));
  if (opts_.server_config.op_exec_worker_num() > 0) {
    exec_opts.op_exec_workers_num = opts_.server_config.op_exec_worker_num();
  }

  if (!opts_.server_config.feature_mapping().empty()) {
    exec_opts.feature_mapping = {opts_.server_config.feature_mapping().begin(),
                                 opts_.server_config.feature_mapping().end()};
  }
  exec_opts.feature_source_config = opts_.feature_source_config;
  auto execution_core = std::make_shared<ExecutionCore>(std::move(exec_opts));

  // start mertrics server
  if (opts_.server_config.metrics_exposer_port() > 0) {
    auto metrics_listen_address =
        fmt::format("{}:{}", host, opts_.server_config.metrics_exposer_port());

    brpc::ServerOptions metrics_server_options;
    if (opts_.server_config.has_tls_config()) {
      SetServerTLSOpts(opts_.server_config.tls_config(),
                       metrics_server_options.mutable_ssl_options());
    }
    if (opts_.server_config.worker_num() > 0) {
      metrics_server_options.num_threads = opts_.server_config.worker_num();
    }

    auto* metrics_service = new metrics::MetricsService();
    metrics_service->RegisterCollectable(metrics::GetDefaultRegistry());
    metrics_server_.set_version(SERVING_VERSION_STRING);
    if (metrics_server_.AddService(metrics_service,
                                   brpc::SERVER_OWNS_SERVICE) != 0) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "fail to add metrics service into brpc server.");
    }

    if (metrics_server_.Start(metrics_listen_address.c_str(),
                              &metrics_server_options) != 0) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "fail to start metrics server at {}",
                    metrics_listen_address);
    }

    SPDLOG_INFO("begin metrics service listen at {}, ", metrics_listen_address);
  }

  // build model_info_collector
  ModelInfoCollector::Options m_c_opts;
  m_c_opts.model_bundle = model_bundle;
  m_c_opts.service_id = opts_.service_id;
  m_c_opts.self_party_id = self_party_id;
  m_c_opts.remote_channel_map = channels;
  ModelInfoCollector model_info_collector(std::move(m_c_opts));
  {
    auto max_retry_cnt =
        opts_.cluster_config.channel_desc().handshake_max_retry_cnt();
    if (max_retry_cnt != 0) {
      model_info_collector.SetRetryCounts(max_retry_cnt);
    }
    auto retry_interval_ms =
        opts_.cluster_config.channel_desc().handshake_retry_interval_ms();
    if (retry_interval_ms != 0) {
      model_info_collector.SetRetryIntervalMs(retry_interval_ms);
    }
  }

  // start commnication server
  std::map<std::string, ModelInfo> model_info_map = {
      {opts_.service_id, model_info_collector.GetSelfModelInfo()}};
  model_service_ = std::make_unique<ModelServiceImpl>(std::move(model_info_map),
                                                      self_party_id);
  {
    brpc::ServerOptions com_server_options;
    if (opts_.server_config.worker_num() > 0) {
      com_server_options.num_threads = opts_.server_config.worker_num();
    }
    if (opts_.server_config.has_tls_config()) {
      SetServerTLSOpts(opts_.server_config.tls_config(),
                       com_server_options.mutable_ssl_options());
    }
    auto* execution_service = new ExecutionServiceImpl(execution_core);
    if (communication_server_.AddService(execution_service,
                                         brpc::SERVER_OWNS_SERVICE) != 0) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "fail to add execution service into brpc server.");
    }
    if (communication_server_.AddService(
            model_service_.get(), brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "fail to add model service into com brpc server.");
    }
    // start services server
    communication_server_.set_version(SERVING_VERSION_STRING);
    if (communication_server_.Start(com_address.c_str(), &com_server_options) !=
        0) {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "fail to start communication brpc server at {}",
                    com_address);
    }

    SPDLOG_INFO("begin communication server listen at {}, ", com_address);
  }

  // start service server
  brpc::ServerOptions server_options;
  server_options.max_concurrency = opts_.server_config.max_concurrency();
  if (opts_.server_config.worker_num() > 0) {
    server_options.num_threads = opts_.server_config.worker_num();
  }
  if (opts_.server_config.brpc_builtin_service_port() > 0) {
    server_options.has_builtin_services = true;
    server_options.internal_port =
        opts_.server_config.brpc_builtin_service_port();
    SPDLOG_INFO("brpc built-in service port: {}", server_options.internal_port);
  }
  if (opts_.server_config.has_tls_config()) {
    SetServerTLSOpts(opts_.server_config.tls_config(),
                     server_options.mutable_ssl_options());
  }
  health::ServingHealthReporter hr;
  server_options.health_reporter = &hr;
  // FIXME:
  // kuscia场景需要在服务启动后，使服务状态可用，此时才能挂载路由。但服务需要完成
  // exchange model info 才能 ready
  hr.SetStatusCode(200);

  if (service_server_.AddService(model_service_.get(),
                                 brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add model service into brpc server.");
  }
  auto* prediction_service = new PredictionServiceImpl(self_party_id);
  if (service_server_.AddService(prediction_service,
                                 brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add prediction service into brpc server.");
  }
  service_server_.set_version(SERVING_VERSION_STRING);
  if (service_server_.Start(service_address.c_str(), &server_options) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to start service brpc server at {}", service_address);
  }

  SPDLOG_INFO("begin service server listen at {}, ", service_address);

  // exchange model info
  SPDLOG_INFO("start exchange model_info");

  model_info_collector.DoCollect();
  auto specific_map = model_info_collector.GetSpecificMap();

  SPDLOG_INFO("end exchange model_info");

  // build prediction core, let prediction service begin to serve
  Predictor::Options predictor_opts;
  predictor_opts.party_id = self_party_id;
  predictor_opts.channels = channels;
  predictor_opts.executions = graph.GetExecutions();
  predictor_opts.specific_party_map = std::move(specific_map);
  auto predictor = std::make_shared<Predictor>(predictor_opts);
  predictor->SetExecutionCore(execution_core);

  PredictionCore::Options prediction_core_opts;
  prediction_core_opts.service_id = opts_.service_id;
  prediction_core_opts.party_id = self_party_id;
  prediction_core_opts.cluster_ids = std::move(cluster_ids);
  prediction_core_opts.predictor = predictor;
  auto prediction_core =
      std::make_shared<PredictionCore>(std::move(prediction_core_opts));
  prediction_service->Init(prediction_core);
}

void Server::WaitForEnd() {
  service_server_.RunUntilAskedToQuit();
  communication_server_.RunUntilAskedToQuit();
  metrics_server_.RunUntilAskedToQuit();
}

}  // namespace secretflow::serving
