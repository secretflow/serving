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

#include "secretflow_serving/server/metrics/metrics_service.h"

//
#include <butil/iobuf.h>
//
#include <brpc/builtin/prometheus_metrics_service.h>
#include <brpc/closure_guard.h>
#include <brpc/controller.h>
#include <prometheus/serializer.h>
#include <prometheus/text_serializer.h>

#include "yacl/utils/elapsed_timer.h"

namespace secretflow::serving::metrics {

MetricsService::MetricsService()
    : exposer_registry_(std::make_shared<::prometheus::Registry>()),
      stats_(std::make_unique<MetricsService::Stats>(exposer_registry_)) {
  RegisterCollectable(exposer_registry_);
}

void MetricsService::default_method(
    ::google::protobuf::RpcController* cntl_base,
    const apis::MetricsRequest* request, apis::MetricsResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  auto cntl = static_cast<brpc::Controller*>(cntl_base);

  yacl::ElapsedTimer timer;

  // write response
  cntl->http_response().set_content_type("text/plain");
  auto& response_io_buf = cntl->response_attachment();

  // brpc metrics
  if (brpc::DumpPrometheusMetricsToIOBuf(&response_io_buf) != 0) {
    cntl->SetFailed("Fail to dump metrics");
    return;
  }
  // serving metrics
  {
    auto metrics = CollectMetrics();
    auto serializer = std::unique_ptr<::prometheus::Serializer>{
        new ::prometheus::TextSerializer()};
    butil::IOBufBuilder body;
    serializer->Serialize(body, metrics);
    response_io_buf.append(body.buf());
  }

  // stat metrics of request `/metrics`
  stats_->request_latencies_s.Observe(timer.CountMs());
  auto bodySize = cntl->response_attachment().size();
  stats_->bytes_transferred.Increment(bodySize);
  stats_->num_scrapes.Increment();
}

void MetricsService::RegisterCollectable(
    const std::weak_ptr<::prometheus::Collectable>& collectable) {
  std::lock_guard<bthread::Mutex> lock(collectables_mutex_);

  collectables_.push_back(collectable);
}

std::vector<::prometheus::MetricFamily> MetricsService::CollectMetrics() {
  auto collected_metrics = std::vector<::prometheus::MetricFamily>{};
  std::unordered_set<size_t> expired_indexes;

  std::lock_guard<bthread::Mutex> lock(collectables_mutex_);

  for (size_t i = 0; i != collectables_.size(); ++i) {
    auto collectable = collectables_[i].lock();
    if (!collectable) {
      expired_indexes.insert(i);
      continue;
    }

    auto&& metrics = collectable->Collect();
    collected_metrics.insert(collected_metrics.end(),
                             std::make_move_iterator(metrics.begin()),
                             std::make_move_iterator(metrics.end()));
  }

  if (!expired_indexes.empty()) {
    std::vector<std::weak_ptr<::prometheus::Collectable>> not_expired;
    for (size_t i = 0; i != collectables_.size(); ++i) {
      if (expired_indexes.find(i) == expired_indexes.end()) {
        not_expired.emplace_back(std::move(collectables_[i]));
      }
    }
    collectables_.swap(not_expired);
  }

  return collected_metrics;
}

MetricsService::Stats::Stats(
    const std::shared_ptr<::prometheus::Registry>& registry)
    : bytes_transferred_family(
          ::prometheus::BuildCounter()
              .Name("exposer_transferred_bytes_total")
              .Help("Transferred bytes to metrics services")
              .Register(*registry)),
      bytes_transferred(bytes_transferred_family.Add({})),
      num_scrapes_family(::prometheus::BuildCounter()
                             .Name("exposer_scrapes_total")
                             .Help("Number of times metrics were scraped")
                             .Register(*registry)),
      num_scrapes(num_scrapes_family.Add({})),
      request_latencies_family(
          ::prometheus::BuildSummary()
              .Name("exposer_request_duration_milliseconds")
              .Help("Latencies of serving scrape requests, in milliseconds")
              .Register(*registry)),
      request_latencies_s(request_latencies_family.Add(
          {}, ::prometheus::Summary::Quantiles{
                  {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}})) {}

}  // namespace secretflow::serving::metrics
