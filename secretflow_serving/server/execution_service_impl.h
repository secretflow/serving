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

#include "secretflow_serving/server/execution_core.h"
#include "secretflow_serving/server/metrics/default_metrics_registry.h"

#include "secretflow_serving/apis/execution_service.pb.h"

namespace secretflow::serving {

class ExecutionServiceImpl : public apis::ExecutionService {
 public:
  explicit ExecutionServiceImpl(
      const std::shared_ptr<ExecutionCore>& execution_core);

  void Execute(::google::protobuf::RpcController* controller,
               const apis::ExecuteRequest* request,
               apis::ExecuteResponse* response,
               ::google::protobuf::Closure* done) override;

 private:
  void RecordMetrics(const apis::ExecuteRequest& request,
                     const apis::ExecuteResponse& response, double duration_ms,
                     const std::string& action);
  struct Stats {
    // for service interface
    ::prometheus::Family<::prometheus::Counter>& api_request_counter_family;
    ::prometheus::Family<::prometheus::Summary>&
        api_request_duration_summary_family;

    Stats(std::map<std::string, std::string> labels,
          const std::shared_ptr<::prometheus::Registry>& registry =
              metrics::GetDefaultRegistry());
  };

 private:
  std::shared_ptr<ExecutionCore> execution_core_;
  Stats stats_;
};

}  // namespace secretflow::serving
