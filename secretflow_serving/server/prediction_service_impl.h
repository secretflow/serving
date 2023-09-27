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

#include "secretflow_serving/server/metrics/default_metrics_registry.h"
#include "secretflow_serving/server/prediction_core.h"

#include "secretflow_serving/apis/prediction_service.pb.h"

namespace secretflow::serving {

// 预测 - 服务入口
class PredictionServiceImpl : public apis::PredictionService {
 public:
  explicit PredictionServiceImpl(
      const std::shared_ptr<PredictionCore>& prediction_core);

  void Predict(::google::protobuf::RpcController* controller,
               const apis::PredictRequest* request,
               apis::PredictResponse* response,
               ::google::protobuf::Closure* done) override;

 private:
  struct Stats {
    // for request api
    ::prometheus::Family<::prometheus::Counter>& api_request_counter_family;
    ::prometheus::Family<::prometheus::Counter>&
        api_request_total_duration_family;
    ::prometheus::Family<::prometheus::Summary>&
        api_request_duration_summary_family;
    ::prometheus::Summary& api_request_duration_summary;
    // for predict sample
    ::prometheus::Family<::prometheus::Counter>& predict_counter_family;
    ::prometheus::Counter& predict_counter;

    explicit Stats(std::map<std::string, std::string> labels,
                   const std::shared_ptr<::prometheus::Registry>& registry =
                       metrics::GetDefaultRegistry());
  };

  void RecordMetrics(const apis::PredictRequest& request,
                     const apis::PredictResponse& response,
                     const double duration_ms);

 private:
  std::shared_ptr<PredictionCore> prediction_core_;
  Stats stats_;
};

}  // namespace secretflow::serving
