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

#include "brpc/controller.h"

#include "secretflow_serving/ops/graph.h"
#include "secretflow_serving/server/execution_core.h"
#include "secretflow_serving/server/trace/trace.h"

#include "secretflow_serving/apis/execution_service.pb.h"
#include "secretflow_serving/apis/prediction_service.pb.h"

namespace secretflow::serving {

inline void MergeResponseHeader(const apis::ExecuteResponse& exec_res,
                                apis::PredictResponse* pred_res) {
  pred_res->mutable_header()->mutable_data()->insert(
      exec_res.header().data().begin(), exec_res.header().data().end());
}

struct ExecuteContext {
  const apis::PredictRequest* pred_req;
  apis::PredictResponse* pred_res;

  std::shared_ptr<Execution> execution;
  std::string local_id;

  apis::ExecuteRequest exec_req;
  apis::ExecuteResponse exec_res;

  ExecuteContext(
      const apis::PredictRequest* request, apis::PredictResponse* response,
      const std::shared_ptr<Execution>& execution, const std::string& local_id,
      const std::unordered_map<std::string, apis::NodeIo>& node_io_map);

  void SetFeatureSource(const std::string& target_id);
};

class ExecuteBase {
 public:
  explicit ExecuteBase(ExecuteContext ctx) : ctx_(std::move(ctx)) {}
  virtual ~ExecuteBase() = default;

  virtual void GetOutputs(
      std::unordered_map<std::string, apis::NodeIo>* node_io_map);

  virtual void Run() = 0;

 protected:
  void SetTarget(const std::string& target_id) {
    target_id_ = target_id;
    ctx_.SetFeatureSource(target_id_);
  }

 protected:
  ExecuteContext ctx_;
  std::string target_id_;
};

class RemoteExecute : public ExecuteBase {
 public:
  RemoteExecute(ExecuteContext ctx, const std::string& target_id,
                ::google::protobuf::RpcChannel* channel);
  virtual ~RemoteExecute();

  void Run() override;

  virtual void Cancel();

  virtual void WaitToFinish();

 protected:
  ::google::protobuf::RpcChannel* channel_;
  brpc::Controller cntl_;

  std::atomic<bool> executing_{false};

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  SpanAttrOption span_option_;
};

class LocalExecute : public ExecuteBase {
 public:
  LocalExecute(ExecuteContext ctx,
               std::shared_ptr<ExecutionCore> execution_core);

  void Run() override;

 protected:
  std::shared_ptr<ExecutionCore> execution_core_;
};

}  // namespace secretflow::serving
