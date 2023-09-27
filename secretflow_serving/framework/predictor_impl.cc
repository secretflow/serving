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

#include "secretflow_serving/framework/predictor_impl.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace {
class OnRPCDone : public google::protobuf::Closure {
 public:
  OnRPCDone(const std::shared_ptr<brpc::Controller>& cntl) : cntl_(cntl) {}

  void Run() override {
    std::unique_ptr<OnRPCDone> self_guard(this);
    // reduce cntl_ reference count
    cntl_ = nullptr;
  }

 private:
  std::shared_ptr<brpc::Controller> cntl_;
};
}  // namespace

PredictorImpl::PredictorImpl(Options opts) : Predictor(std::move(opts)) {}

void PredictorImpl::Predict(const apis::PredictRequest* request,
                            apis::PredictResponse* response) {
  std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>> last_exec_ctxs;
  for (const auto& e : opts_.executions) {
    if (e->GetDispatchType() == DispatchType::DP_ALL) {
      // async exec peers
      auto peer_ctx_list =
          std::make_shared<std::vector<std::shared_ptr<ExecuteContext>>>();
      for (const auto& [party_id, _] : *opts_.channels) {
        auto ctx = BuildExecCtx(request, response, party_id, e, last_exec_ctxs);
        peer_ctx_list->emplace_back(ctx);
      }
      AsyncPeersExecute(peer_ctx_list);

      // exec self
      auto local_ctx =
          BuildExecCtx(request, response, opts_.party_id, e, last_exec_ctxs);
      LocalExecute(local_ctx, peer_ctx_list);

      // join peers
      JoinPeersExecute(peer_ctx_list);

      last_exec_ctxs = peer_ctx_list;
      last_exec_ctxs->emplace_back(local_ctx);
    } else if (e->GetDispatchType() == DispatchType::DP_ANYONE) {
      auto local_ctx =
          BuildExecCtx(request, response, opts_.party_id, e, last_exec_ctxs);
      LocalExecute(local_ctx);
      last_exec_ctxs =
          std::make_shared<std::vector<std::shared_ptr<ExecuteContext>>>();
      last_exec_ctxs->emplace_back(local_ctx);
    } else {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "unsupport dispatch type: {}",
                    DispatchType_Name(e->GetDispatchType()));
    }
  }

  SERVING_ENFORCE(last_exec_ctxs->size() == 1, errors::ErrorCode::LOGIC_ERROR);
  DealFinalResult(last_exec_ctxs->front(), response);
}

std::shared_ptr<PredictorImpl::ExecuteContext> PredictorImpl::BuildExecCtx(
    const apis::PredictRequest* request, apis::PredictResponse* response,
    const std::string& target_id, const std::shared_ptr<Execution>& execution,
    std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
        last_exec_ctxs) {
  auto ctx = std::make_shared<ExecuteContext>();
  ctx->response = response;
  ctx->request = request;

  ctx->target_id = target_id;
  ctx->execution = execution;

  ctx->exec_res = std::make_shared<apis::ExecuteResponse>();
  ctx->cntl = std::make_shared<brpc::Controller>();

  ctx->exec_req = std::make_shared<apis::ExecuteRequest>();
  ctx->exec_req->mutable_header()->CopyFrom(request->header());
  ctx->exec_req->set_requester_id(opts_.party_id);
  ctx->exec_req->mutable_service_spec()->CopyFrom(request->service_spec());
  auto feature_source = ctx->exec_req->mutable_feature_source();
  if (execution->IsEntry()) {
    // entry execution need features
    // get target_id's feature param
    if (target_id == opts_.party_id &&
        request->predefined_features_size() != 0) {
      // only loacl execute will use `predefined_features`
      feature_source->set_type(apis::FeatureSourceType::FS_PREDEFINED);
      feature_source->mutable_predefineds()->CopyFrom(
          request->predefined_features());
    } else {
      feature_source->set_type(apis::FeatureSourceType::FS_SERVICE);
      auto iter = request->fs_params().find(target_id);
      SERVING_ENFORCE(iter != request->fs_params().end(),
                      errors::ErrorCode::INVALID_ARGUMENT,
                      "missing {}'s feature params", target_id);
      feature_source->mutable_fs_param()->CopyFrom(iter->second);
    }
  } else {
    feature_source->set_type(apis::FeatureSourceType::FS_NONE);
  }
  // build task
  auto task = ctx->exec_req->mutable_task();
  task->set_execution_id(execution->id());
  if (last_exec_ctxs) {
    // merge output node_io
    std::map<std::string, std::shared_ptr<apis::NodeIo>> node_io_map;
    for (auto& res_ctx : *last_exec_ctxs) {
      auto result = res_ctx->exec_res->mutable_result();
      for (int i = 0; i < result->nodes_size(); ++i) {
        auto node_io = result->mutable_nodes(i);
        auto iter = node_io_map.find(node_io->name());
        if (iter != node_io_map.end()) {
          // found node, merge ios
          auto& target_node_io = iter->second;
          SERVING_ENFORCE(target_node_io->ios_size() == node_io->ios_size(),
                          errors::ErrorCode::LOGIC_ERROR);
          for (int io_index = 0; io_index < target_node_io->ios_size();
               ++io_index) {
            auto target_io = target_node_io->mutable_ios(io_index);
            auto io = node_io->mutable_ios(io_index);
            for (int data_index = 0; data_index < io->datas_size();
                 ++data_index)
              target_io->add_datas(std::move(*(io->mutable_datas(data_index))));
          }
        } else {
          auto node_name = node_io->name();
          node_io_map.emplace(
              node_name, std::make_shared<apis::NodeIo>(std::move(*node_io)));
        }
      }
    }
    // build intput from last output
    auto entry_nodes = execution->GetEntryNodes();
    for (const auto& n : entry_nodes) {
      auto entry_node_io = task->add_nodes();
      entry_node_io->set_name(n->GetName());
      for (const auto& e : n->in_edges()) {
        auto iter = node_io_map.find(e->src_node());
        SERVING_ENFORCE(iter != node_io_map.end(),
                        errors::ErrorCode::LOGIC_ERROR);
        for (auto& io : *(iter->second->mutable_ios())) {
          entry_node_io->mutable_ios()->Add(std::move(io));
        }
      }
    }
  }
  return ctx;
}

void PredictorImpl::AsyncPeersExecute(
    std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
        context_list) {
  for (size_t i = 0; i < context_list->size(); ++i) {
    auto ctx = context_list->at(i);

    AsyncCallRpc(ctx->target_id, ctx->cntl, ctx->exec_req.get(),
                 ctx->exec_res.get());
  }
}

void PredictorImpl::JoinPeersExecute(
    std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
        context_list) {
  for (auto& context : *context_list) {
    JoinAsyncCall(context->target_id, context->cntl);
  }
  for (auto& context : *context_list) {
    MergeHeader(context->response, context->exec_res);
    CheckExecResponse(context->target_id, context->exec_res);
  }
}

void PredictorImpl::SyncPeersExecute(
    std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
        context_list) {
  AsyncPeersExecute(context_list);
  JoinPeersExecute(context_list);
}

void PredictorImpl::AsyncCallRpc(const std::string& target_id,
                                 std::shared_ptr<brpc::Controller>& cntl,
                                 const apis::ExecuteRequest* request,
                                 apis::ExecuteResponse* response) {
  OnRPCDone* done = new OnRPCDone(cntl);
  // semisynchronous call
  apis::ExecutionService_Stub stub(opts_.channels->at(target_id).get());
  stub.Execute(cntl.get(), request, response, done);
}

void PredictorImpl::JoinAsyncCall(
    const std::string& target_id,
    const std::shared_ptr<brpc::Controller>& cntl) {
  brpc::Join(cntl->call_id());
  SERVING_ENFORCE(!cntl->Failed(), errors::ErrorCode::NETWORK_ERROR,
                  "call ({}) execute failed, msg:{}", target_id,
                  cntl->ErrorText());
}

void PredictorImpl::CancelAsyncCall(
    const std::shared_ptr<brpc::Controller>& cntl) {
  brpc::StartCancel(cntl->call_id());
}

void PredictorImpl::LocalExecute(
    std::shared_ptr<ExecuteContext>& context,
    std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>
        exception_cancel_cxts) {
  try {
    context->exec_res = std::make_shared<apis::ExecuteResponse>();
    execution_core_->Execute(context->exec_req.get(), context->exec_res.get());
    MergeHeader(context->response, context->exec_res);
    CheckExecResponse(context->target_id, context->exec_res);
  } catch (Exception& e) {
    if (exception_cancel_cxts) {
      for (const auto& cxt : *exception_cancel_cxts) {
        CancelAsyncCall(cxt->cntl);
      }
    }
    throw e;
  } catch (std::exception& e) {
    if (exception_cancel_cxts) {
      for (const auto& cxt : *exception_cancel_cxts) {
        CancelAsyncCall(cxt->cntl);
      }
    }
    throw e;
  }
}

void PredictorImpl::MergeHeader(
    apis::PredictResponse* response,
    const std::shared_ptr<apis::ExecuteResponse>& exec_response) {
  response->mutable_header()->mutable_data()->insert(
      exec_response->header().data().begin(),
      exec_response->header().data().end());
}

void PredictorImpl::CheckExecResponse(
    const std::string& party_id,
    const std::shared_ptr<apis::ExecuteResponse>& response) {
  if (!CheckStatusOk(response->status())) {
    SERVING_THROW(
        response->status().code(),
        fmt::format("{} exec failed: {}", party_id, response->status().msg()));
  }
}

void PredictorImpl::DealFinalResult(std::shared_ptr<ExecuteContext>& ctx,
                                    apis::PredictResponse* response) {
  std::shared_ptr<arrow::RecordBatch> record_batch;
  auto exec_result = ctx->exec_res->result();
  SERVING_ENFORCE(exec_result.nodes_size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);
  for (const auto& n : exec_result.nodes()) {
    SERVING_ENFORCE(n.ios_size() == 1, errors::ErrorCode::LOGIC_ERROR);
    for (const auto& io : n.ios()) {
      SERVING_ENFORCE(io.datas_size() == 1, errors::ErrorCode::LOGIC_ERROR);
      record_batch = DeserializeRecordBatch(io.datas(0));
      break;
    }
    break;
  }

  for (int64_t i = 0; i < record_batch->num_rows(); ++i) {
    auto result = response->add_results();
    for (int j = 0; j < record_batch->num_columns(); ++j) {
      auto field = record_batch->schema()->field(j);
      auto array = record_batch->column(j);
      std::shared_ptr<arrow::Scalar> raw_scalar;
      SERVING_GET_ARROW_RESULT(record_batch->column(j)->GetScalar(i),
                               raw_scalar);
      std::shared_ptr<arrow::Scalar> scalar = raw_scalar;
      if (raw_scalar->type->id() != arrow::Type::type::DOUBLE) {
        // cast type
        SERVING_GET_ARROW_RESULT(raw_scalar->CastTo(arrow::float64()), scalar);
      }
      auto score_value =
          std::static_pointer_cast<arrow::DoubleScalar>(scalar)->value;
      auto score = result->add_scores();
      score->set_name(field->name());
      score->set_value(score_value);
    }
  }
}

}  // namespace secretflow::serving
