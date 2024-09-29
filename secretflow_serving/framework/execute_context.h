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

void ExeResponseToIoMap(
    apis::ExecuteResponse& exec_res,
    std::unordered_map<std::string, apis::NodeIo>* node_io_map);

class ExecuteContext {
 public:
  ExecuteContext(const apis::PredictRequest* request,
                 apis::PredictResponse* response,
                 const std::shared_ptr<Execution>& execution,
                 std::string target_id, std::string local_id);

  template <
      typename T,
      typename = std::enable_if_t<std::is_same_v<
          std::decay_t<T>, std::unordered_map<std::string, apis::NodeIo>>>>
  void SetEntryNodesInputs(T&& node_io_map) {
    if (node_io_map.empty()) {
      return;
    }
    auto* task = exec_req_.mutable_task();
    task->set_execution_id(execution_->id());
    auto entry_nodes = execution_->GetEntryNodes();
    for (const auto& n : entry_nodes) {
      auto* entry_node_io = task->add_nodes();
      entry_node_io->set_name(n->GetName());
      for (const auto& e : n->in_edges()) {
        auto iter = node_io_map.find(e->src_node());
        SERVING_ENFORCE(iter != node_io_map.end(),
                        errors::ErrorCode::LOGIC_ERROR,
                        "Input of {} cannot be found in ctx(size:{})",
                        e->src_node(), node_io_map.size());
        for (auto& io : *(iter->second.mutable_ios())) {
          if constexpr (std::is_lvalue_reference_v<T&&>) {
            *(entry_node_io->mutable_ios()->Add()) = io;
          } else {
            entry_node_io->mutable_ios()->Add(std::move(io));
          }
        }
      }
    }
  }

  void Execute(::google::protobuf::RpcChannel* channel, brpc::Controller* cntl);
  void Execute(std::shared_ptr<ExecutionCore>& execution_core);

  void GetResultNodeIo(
      std::unordered_map<std::string, apis::NodeIo>* node_io_map);

  void CheckAndUpdateResponse(const apis::ExecuteResponse& exec_res);
  void CheckAndUpdateResponse();

  const apis::Status& ResponseStatus() const { return exec_res_.status(); }

  void MergeResonseHeader(const apis::ExecuteResponse& exec_res);
  void MergeResonseHeader();

  const std::string& LocalId() const { return local_id_; }
  const std::string& TargetId() const { return target_id_; }
  const std::string& ServiceId() const { return request_->service_spec().id(); }
  apis::ExecuteRequest& ExecReq() { return exec_req_; }
  const apis::ExecuteRequest& ExecReq() const { return exec_req_; }

 private:
  void SetFeatureSource();

 protected:
  const apis::PredictRequest* request_;
  apis::PredictResponse* response_;

  std::string local_id_;
  std::string target_id_;
  std::shared_ptr<Execution> execution_;

  std::string session_id_;

  apis::ExecuteRequest exec_req_;
  apis::ExecuteResponse exec_res_;
};

class ExecuteBase {
 public:
  ExecuteBase(const apis::PredictRequest* request,
              apis::PredictResponse* response,
              const std::shared_ptr<Execution>& execution,
              std::string target_id, std::string local_id)
      : exec_ctx_{request, response, execution, std::move(target_id),
                  std::move(local_id)} {}
  virtual ~ExecuteBase() = default;

  void SetInputs(std::unordered_map<std::string, apis::NodeIo>& node_io_map) {
    exec_ctx_.SetEntryNodesInputs(node_io_map);
  }
  void SetInputs(std::unordered_map<std::string, apis::NodeIo>&& node_io_map) {
    exec_ctx_.SetEntryNodesInputs(std::move(node_io_map));
  }
  virtual void GetOutputs(
      std::unordered_map<std::string, apis::NodeIo>* node_io_map) {
    exec_ctx_.GetResultNodeIo(node_io_map);
  }

  virtual void Run() = 0;

 protected:
  ExecuteContext exec_ctx_;
};

class RemoteExecute : public ExecuteBase {
 public:
  RemoteExecute(const apis::PredictRequest* request,
                apis::PredictResponse* response,
                const std::shared_ptr<Execution>& execution,
                std::string target_id, std::string local_id,
                ::google::protobuf::RpcChannel* channel);

  virtual ~RemoteExecute();

  void Run() override;

  virtual void Cancel();

  virtual void WaitToFinish();

 protected:
  ::google::protobuf::RpcChannel* channel_;
  brpc::Controller cntl_;

  bool executing_{false};

  opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span_;
  SpanAttrOption span_option;
};

class LocalExecute : public ExecuteBase {
 public:
  LocalExecute(const apis::PredictRequest* request,
               apis::PredictResponse* response,
               const std::shared_ptr<Execution>& execution,
               std::string target_id, std::string local_id,
               std::shared_ptr<ExecutionCore> execution_core)
      : ExecuteBase{request, response, execution, std::move(target_id),
                    std::move(local_id)},
        execution_core_(std::move(execution_core)) {}

  void Run() override {
    exec_ctx_.Execute(execution_core_);
    exec_ctx_.CheckAndUpdateResponse();
  }

 protected:
  std::shared_ptr<ExecutionCore> execution_core_;
};

}  // namespace secretflow::serving
