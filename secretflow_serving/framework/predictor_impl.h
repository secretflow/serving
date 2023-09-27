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

#include "secretflow_serving/framework/predictor.h"
#include "secretflow_serving/ops/graph.h"

#include "secretflow_serving/apis/execution_service.pb.h"

namespace secretflow::serving {

class PredictorImpl : public Predictor {
 public:
  struct ExecuteContext {
    const apis::PredictRequest* request;
    apis::PredictResponse* response;

    std::string session_id;
    std::string target_id;
    std::shared_ptr<Execution> execution;

    std::shared_ptr<apis::ExecuteRequest> exec_req;
    std::shared_ptr<apis::ExecuteResponse> exec_res;
    std::shared_ptr<brpc::Controller> cntl;
  };

 public:
  explicit PredictorImpl(Options opts);
  ~PredictorImpl() = default;

  void Predict(const apis::PredictRequest* request,
               apis::PredictResponse* response) override;

 protected:
  std::shared_ptr<ExecuteContext> BuildExecCtx(
      const apis::PredictRequest* request, apis::PredictResponse* response,
      const std::string& target_id, const std::shared_ptr<Execution>& execution,
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
          last_exec_ctxs);

  void AsyncPeersExecute(
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
          context_list);
  void JoinPeersExecute(
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
          context_list);
  void SyncPeersExecute(
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
          context_list);

  void LocalExecute(
      std::shared_ptr<ExecuteContext>& context,
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>
          exception_cancel_cxts = nullptr);

  virtual void AsyncCallRpc(const std::string& target_id,
                            std::shared_ptr<brpc::Controller>& cntl,
                            const apis::ExecuteRequest* request,
                            apis::ExecuteResponse* response);

  virtual void JoinAsyncCall(const std::string& target_id,
                             const std::shared_ptr<brpc::Controller>& cntl);

  virtual void CancelAsyncCall(const std::shared_ptr<brpc::Controller>& cntl);

  void MergeHeader(apis::PredictResponse* response,
                   const std::shared_ptr<apis::ExecuteResponse>& exec_response);

  void CheckExecResponse(
      const std::string& party_id,
      const std::shared_ptr<apis::ExecuteResponse>& response);

  void DealFinalResult(std::shared_ptr<ExecuteContext>& ctx,
                       apis::PredictResponse* response);
};

}  // namespace secretflow::serving
