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
#include "secretflow_serving/server/model_service_impl.h"
#include "secretflow_serving/server/prediction_service_impl.h"
#include "secretflow_serving/server/version.h"
#include "secretflow_serving/source/factory.h"
#include "secretflow_serving/util/network.h"

#include "secretflow_serving/apis/execution_service.pb.h"
#include "secretflow_serving/apis/metrics.pb.h"
#include "secretflow_serving/apis/prediction_service.pb.h"

DEFINE_bool(enable_peers_load_balancer, false,
            "whether to enable load balancer between parties");

namespace secretflow::serving {

namespace {

const int32_t kPeerConnectTimeoutMs = 500;
const int32_t kPeerRpcTimeoutMs = 2000;

}  // namespace

Server::Server(Options opts) : opts_(std::move(opts)) {}

Server::~Server() {
  service_server_.Stop(0);
  metrics_server_.Stop(0);
  service_server_.Join();
  metrics_server_.Join();
}

void Server::Start() {
  const auto& self_party_id = opts_.cluster_config.self_id();

  // get model package
  auto source = SourceFactory::GetInstance()->Create(opts_.model_config,
                                                     opts_.service_id);
  auto package_path = source->PullModel();

  // build channels
  std::string self_address;
  std::vector<std::string> cluster_ids;
  auto channels = std::make_shared<PartyChannelMap>();
  for (const auto& party : opts_.cluster_config.parties()) {
    cluster_ids.emplace_back(party.id());
    if (party.id() == self_party_id) {
      self_address = party.listen_address().empty() ? party.address()
                                                    : party.listen_address();
      continue;
    }
    channels->emplace(
        party.id(),
        CreateBrpcChannel(
            party.address(), opts_.cluster_config.channel_desc().protocol(),
            FLAGS_enable_peers_load_balancer,
            opts_.cluster_config.channel_desc().rpc_timeout_ms() > 0
                ? opts_.cluster_config.channel_desc().rpc_timeout_ms()
                : kPeerRpcTimeoutMs,
            opts_.cluster_config.channel_desc().connect_timeout_ms() > 0
                ? opts_.cluster_config.channel_desc().connect_timeout_ms()
                : kPeerConnectTimeoutMs,
            opts_.cluster_config.channel_desc().has_tls_config()
                ? &opts_.cluster_config.channel_desc().tls_config()
                : nullptr));
  }

  // load model package
  auto loader = std::make_unique<ModelLoader>();
  loader->Load(package_path);
  const auto& model_bundle = loader->GetModelBundle();
  Graph graph(model_bundle->graph());

  // build execution core
  std::vector<std::shared_ptr<Executor>> executors;
  for (const auto& execution : graph.GetExecutions()) {
    executors.emplace_back(std::make_shared<Executor>(execution));
  }
  ExecutionCore::Options exec_opts;
  exec_opts.id = opts_.service_id;
  exec_opts.party_id = self_party_id;
  exec_opts.executable = std::make_shared<Executable>(std::move(executors));
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
    std::vector<std::string> strs = absl::StrSplit(self_address, ':');
    SERVING_ENFORCE(strs.size() == 2, errors::ErrorCode::LOGIC_ERROR,
                    "invalid self address.");
    auto metrics_listen_address = fmt::format(
        "{}:{}", strs[0], opts_.server_config.metrics_exposer_port());

    brpc::ServerOptions metrics_server_options;
    if (opts_.server_config.has_tls_config()) {
      auto* ssl_opts = metrics_server_options.mutable_ssl_options();
      ssl_opts->default_cert.certificate =
          opts_.server_config.tls_config().certificate_path();
      ssl_opts->default_cert.private_key =
          opts_.server_config.tls_config().private_key_path();
      ssl_opts->verify.verify_depth = 1;
      ssl_opts->verify.ca_file_path =
          opts_.server_config.tls_config().ca_file_path();
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
                    "fail to start metrics server at {}", self_address);
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

  // add services
  auto* model_service = new ModelServiceImpl(
      {{opts_.service_id, model_info_collector.GetSelfModelInfo()}},
      self_party_id);
  if (service_server_.AddService(model_service, brpc::SERVER_OWNS_SERVICE) !=
      0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add model service into brpc server.");
  }
  auto* execution_service = new ExecutionServiceImpl(execution_core);
  if (service_server_.AddService(execution_service,
                                 brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add execution service into brpc server.");
  }
  auto* prediction_service = new PredictionServiceImpl(self_party_id);
  if (service_server_.AddService(prediction_service,
                                 brpc::SERVER_OWNS_SERVICE) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to add prediction service into brpc server.");
  }

  // build services server opts
  brpc::ServerOptions server_options;
  server_options.max_concurrency = opts_.server_config.max_concurrency();
  if (opts_.server_config.worker_num() > 0) {
    server_options.num_threads = opts_.server_config.worker_num();
  }
  if (opts_.server_config.brpc_builtin_service_port() > 0) {
    server_options.has_builtin_services = true;
    server_options.internal_port =
        opts_.server_config.brpc_builtin_service_port();
    SPDLOG_INFO("internal port: {}", server_options.internal_port);
  }
  if (opts_.server_config.has_tls_config()) {
    auto* ssl_opts = server_options.mutable_ssl_options();
    ssl_opts->default_cert.certificate =
        opts_.server_config.tls_config().certificate_path();
    ssl_opts->default_cert.private_key =
        opts_.server_config.tls_config().private_key_path();
    ssl_opts->verify.verify_depth = 1;
    ssl_opts->verify.ca_file_path =
        opts_.server_config.tls_config().ca_file_path();
  }
  health::ServingHealthReporter hr;
  server_options.health_reporter = &hr;

  // start services server
  service_server_.set_version(SERVING_VERSION_STRING);
  if (service_server_.Start(self_address.c_str(), &server_options) != 0) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "fail to start brpc server at {}", self_address);
  }

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

  // set server ready code
  hr.SetStatusCode(200);
}

void Server::WaitForEnd() {
  service_server_.RunUntilAskedToQuit();
  metrics_server_.RunUntilAskedToQuit();
}

}  // namespace secretflow::serving
