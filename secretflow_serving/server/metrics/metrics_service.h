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

#include <prometheus/collectable.h>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <prometheus/summary.h>

#include "bthread/bthread.h"

#include "secretflow_serving/apis/metrics.pb.h"

namespace secretflow::serving::metrics {

class MetricsService : public apis::metrics {
 public:
  void default_method(::google::protobuf::RpcController* cntl_base,
                      const apis::MetricsRequest* request,
                      apis::MetricsResponse* response,
                      ::google::protobuf::Closure* done) override;

  /// should only be called before add this service to brcp::Server
  void RegisterCollectable(
      const std::weak_ptr<::prometheus::Collectable>& collectable);

 public:
  MetricsService();
  virtual ~MetricsService() = default;

 private:
  std::vector<::prometheus::MetricFamily> CollectMetrics();

 private:
  bthread::Mutex collectables_mutex_;
  std::vector<std::weak_ptr<::prometheus::Collectable>> collectables_;

  std::shared_ptr<::prometheus::Registry> exposer_registry_;
  // metrics of `/metrics` service
  struct Stats {
    ::prometheus::Family<::prometheus::Counter>& bytes_transferred_family;
    ::prometheus::Counter& bytes_transferred;
    ::prometheus::Family<::prometheus::Counter>& num_scrapes_family;
    ::prometheus::Counter& num_scrapes;
    ::prometheus::Family<::prometheus::Summary>& request_latencies_family;
    ::prometheus::Summary& request_latencies_s;

    Stats(const std::shared_ptr<::prometheus::Registry>& registry);
  };

  std::unique_ptr<Stats> stats_;
};

}  // namespace secretflow::serving::metrics
