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

#include "prometheus/counter.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"
#include "prometheus/summary.h"

#include "secretflow_serving/feature_adapter/feature_adapter.h"
#include "secretflow_serving/framework/executable.h"
#include "secretflow_serving/server/metrics/default_metrics_registry.h"

#include "secretflow_serving/apis/execution_service.pb.h"

namespace secretflow::serving {

class ExecutionCore {
 public:
  struct Options {
    std::string id;
    std::string party_id;
    std::optional<std::map<std::string, std::string>> feature_mapping;

    std::optional<FeatureSourceConfig> feature_source_config;

    std::shared_ptr<Executable> executable;
  };

 public:
  explicit ExecutionCore(Options opts);
  virtual ~ExecutionCore() = default;

  virtual void Execute(const apis::ExecuteRequest* request,
                       apis::ExecuteResponse* response);

  const std::string& GetServiceID() const { return opts_.id; }
  const std::string& GetPartyID() const { return opts_.party_id; }

 private:
  std::shared_ptr<arrow::RecordBatch> BatchFetchFeatures(
      const apis::ExecuteRequest* request,
      apis::ExecuteResponse* response) const;
  void RecordMetrics(const apis::ExecuteRequest& request,
                     const apis::ExecuteResponse& response, double duration_ms);
  void RecordBatchFeatureMetrics(const std::string& requester_id, int code,
                                 double duration_ms) const;

  std::shared_ptr<arrow::RecordBatch> ApplyFeatureMappingRule(
      const std::shared_ptr<arrow::RecordBatch>& features);

 private:
  const Options opts_;

  bool valid_feature_mapping_flag_;

  std::unique_ptr<feature::FeatureAdapter> feature_adapater_;

  struct Stats {
    // for service interface
    ::prometheus::Family<::prometheus::Counter>& execute_request_counter_family;
    ::prometheus::Family<::prometheus::Counter>&
        execute_request_totol_duration_family;
    ::prometheus::Family<::prometheus::Summary>&
        execute_request_duration_summary_family;
    ::prometheus::Summary& execute_request_duration_summary;

    ::prometheus::Family<::prometheus::Counter>& fetch_feature_counter_family;
    ::prometheus::Family<::prometheus::Counter>&
        fetch_feature_total_duration_family;
    ::prometheus::Family<::prometheus::Summary>&
        fetch_feature_duration_summary_family;
    ::prometheus::Summary& fetch_feature_duration_summary;

    Stats(std::map<std::string, std::string> labels,
          const std::shared_ptr<::prometheus::Registry>& registry =
              metrics::GetDefaultRegistry());
  };
  Stats stats_;
};

}  // namespace secretflow::serving
