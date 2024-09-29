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

#include "secretflow_serving/framework/predictor.h"

#include <iterator>

#include "arrow/compute/api.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

Predictor::Predictor(Options opts) : opts_(std::move(opts)) {}

void Predictor::Predict(const apis::PredictRequest* request,
                        apis::PredictResponse* response) {
  std::unordered_map<std::string, apis::NodeIo> prev_node_io_map;
  std::vector<std::unique_ptr<RemoteExecute>> async_running_execs;
  async_running_execs.reserve(opts_.channels->size());

  auto execute_locally =
      [&](const std::shared_ptr<Execution>& execution,
          std::unordered_map<std::string, apis::NodeIo>&& prev_io_map,
          std::unordered_map<std::string, apis::NodeIo>* cur_io_map) {
        // exec locally
        auto local_exec = BuildLocalExecute(request, response, execution);
        local_exec->SetInputs(std::move(prev_io_map));
        local_exec->Run();
        local_exec->GetOutputs(cur_io_map);
      };

  for (const auto& e : opts_.executions) {
    async_running_execs.clear();
    std::unordered_map<std::string, apis::NodeIo> new_node_io_map;
    if (e->GetDispatchType() == DispatchType::DP_ALL) {
      // remote exec
      for (const auto& [party_id, channel] : *opts_.channels) {
        auto ctx = BuildRemoteExecute(request, response, e, party_id, channel);
        ctx->SetInputs(std::move(prev_node_io_map));
        ctx->Run();

        async_running_execs.emplace_back(std::move(ctx));
      }

      // exec locally
      if (execution_core_) {
        execute_locally(e, std::move(prev_node_io_map), &new_node_io_map);
      } else {
        // TODO: support no execution core scene
        SERVING_THROW(errors::ErrorCode::NOT_IMPLEMENTED, "not implemented");
      }

      // join async exec
      for (const auto& exec : async_running_execs) {
        exec->WaitToFinish();
        exec->GetOutputs(&new_node_io_map);
      }

    } else if (e->GetDispatchType() == DispatchType::DP_ANYONE) {
      // exec locally
      if (execution_core_) {
        execute_locally(e, std::move(prev_node_io_map), &new_node_io_map);
      } else {
        // TODO: support no execution core scene
        SERVING_THROW(errors::ErrorCode::NOT_IMPLEMENTED, "not implemented");
      }
    } else if (e->GetDispatchType() == DispatchType::DP_SPECIFIED) {
      if (e->SpecificToThis()) {
        SERVING_ENFORCE(execution_core_, errors::ErrorCode::UNEXPECTED_ERROR);
        execute_locally(e, std::move(prev_node_io_map), &new_node_io_map);
      } else {
        auto iter = opts_.specific_party_map.find(e->id());
        SERVING_ENFORCE(iter != opts_.specific_party_map.end(),
                        serving::errors::LOGIC_ERROR,
                        "{} execution assign to no party", e->id());
        auto ctx = BuildRemoteExecute(request, response, e, iter->second,
                                      opts_.channels->at(iter->second));
        ctx->SetInputs(std::move(prev_node_io_map));
        ctx->Run();
        ctx->WaitToFinish();
        ctx->GetOutputs(&new_node_io_map);
      }
    } else {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "unsupported dispatch type: {}",
                    DispatchType_Name(e->GetDispatchType()));
    }
    prev_node_io_map.swap(new_node_io_map);
  }

  DealFinalResult(prev_node_io_map, response);
}

std::unique_ptr<RemoteExecute> Predictor::BuildRemoteExecute(
    const apis::PredictRequest* request, apis::PredictResponse* response,
    const std::shared_ptr<Execution>& execution, std::string target_id,
    const std::unique_ptr<::google::protobuf::RpcChannel>& channel) {
  return std::make_unique<RemoteExecute>(request, response, execution,
                                         std::move(target_id), opts_.party_id,
                                         channel.get());
}

std::unique_ptr<LocalExecute> Predictor::BuildLocalExecute(
    const apis::PredictRequest* request, apis::PredictResponse* response,
    const std::shared_ptr<Execution>& execution) {
  return std::make_unique<LocalExecute>(request, response, execution,
                                        opts_.party_id, opts_.party_id,
                                        execution_core_);
}

void Predictor::DealFinalResult(
    std::unordered_map<std::string, apis::NodeIo>& node_io_map,
    apis::PredictResponse* response) {
  SERVING_ENFORCE(node_io_map.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  auto& node_io = node_io_map.begin()->second;
  SERVING_ENFORCE(node_io.ios_size() == 1, errors::ErrorCode::LOGIC_ERROR);
  const auto& ios = node_io.ios(0);
  SERVING_ENFORCE(ios.datas_size() == 1, errors::ErrorCode::LOGIC_ERROR);
  std::shared_ptr<arrow::RecordBatch> record_batch =
      DeserializeRecordBatch(ios.datas(0));

  std::vector<apis::PredictResult*> results(record_batch->num_rows());
  for (int64_t i = 0; i != record_batch->num_rows(); ++i) {
    results[i] = response->add_results();
  }

  for (int j = 0; j < record_batch->num_columns(); ++j) {
    auto col_vector = CastToDoubleArray(record_batch->column(j));
    auto field_name = record_batch->schema()->field(j)->name();
    for (int64_t i = 0; i < record_batch->num_rows(); ++i) {
      auto* score = results[i]->add_scores();
      score->set_name(field_name);
      score->set_value(col_vector->Value(i));
    }
  }
}

}  // namespace secretflow::serving
