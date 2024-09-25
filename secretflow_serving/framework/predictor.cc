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

namespace {
void DealFinalResult(apis::NodeIo& node_io, const apis::PredictRequest* request,
                     const std::string& party_id,
                     apis::PredictResponse* response) {
  SERVING_ENFORCE(node_io.ios_size() == 1, errors::ErrorCode::LOGIC_ERROR);
  const auto& ios = node_io.ios(0);
  SERVING_ENFORCE(ios.datas_size() == 1, errors::ErrorCode::LOGIC_ERROR);

  std::shared_ptr<arrow::RecordBatch> record_batch =
      DeserializeRecordBatch(ios.datas(0));

  int64_t sample_num = 0;
  if (request->predefined_features_size() > 0) {
    sample_num = CountSampleNum(request->predefined_features());
  } else {
    sample_num = request->fs_params().find(party_id)->second.query_datas_size();
  }
  SERVING_ENFORCE_EQ(
      record_batch->num_rows(), sample_num,
      "The number of calculated results does not match the number of requests");

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
}  // namespace

Predictor::Predictor(Options opts) : opts_(std::move(opts)) {}

void Predictor::Predict(const apis::PredictRequest* request,
                        apis::PredictResponse* response) {
  auto execute_locally =
      [&](ExecuteContext ctx,
          std::unordered_map<std::string, apis::NodeIo>* io_map) {
        // exec locally
        LocalExecute exec(std::move(ctx), execution_core_);
        exec.Run();
        exec.GetOutputs(io_map);
      };

  std::unordered_map<std::string, apis::NodeIo> node_io_map;
  std::vector<std::unique_ptr<RemoteExecute>> async_running_execs;
  async_running_execs.reserve(opts_.channels->size());

  std::string exit_node_name;
  for (const auto& e : opts_.executions) {
    async_running_execs.clear();
    if (e->IsGraphExit()) {
      exit_node_name = e->GetExitNodes().front()->GetName();
    }
    ExecuteContext ctx(request, response, e, opts_.party_id, node_io_map);
    if (e->GetDispatchType() == DispatchType::DP_ALL) {
      // remote exec
      for (const auto& [party_id, channel] : *opts_.channels) {
        auto exec = BuildRemoteExecute(ctx, party_id, channel);
        exec->Run();
        async_running_execs.emplace_back(std::move(exec));
      }

      // exec locally
      if (execution_core_) {
        execute_locally(std::move(ctx), &node_io_map);
      } else {
        // TODO: support no execution core scene
        SERVING_THROW(errors::ErrorCode::NOT_IMPLEMENTED, "not implemented");
      }

      // join async exec
      for (const auto& exec : async_running_execs) {
        exec->WaitToFinish();
        exec->GetOutputs(&node_io_map);
      }

    } else if (e->GetDispatchType() == DispatchType::DP_ANYONE ||
               e->GetDispatchType() == DispatchType::DP_SELF) {
      // exec locally
      if (execution_core_) {
        execute_locally(std::move(ctx), &node_io_map);
      } else {
        // TODO: support no execution core scene
        SERVING_THROW(errors::ErrorCode::NOT_IMPLEMENTED, "not implemented");
      }
    } else if (e->GetDispatchType() == DispatchType::DP_SPECIFIED) {
      if (e->SpecificFlag()) {
        SERVING_ENFORCE(execution_core_, errors::ErrorCode::UNEXPECTED_ERROR);
        execute_locally(std::move(ctx), &node_io_map);
      } else {
        auto iter = opts_.specific_party_map.find(e->id());
        SERVING_ENFORCE(iter != opts_.specific_party_map.end(),
                        serving::errors::LOGIC_ERROR,
                        "{} execution assign to no party", e->id());
        auto exec = BuildRemoteExecute(std::move(ctx), iter->second,
                                       opts_.channels->at(iter->second));
        exec->Run();
        exec->WaitToFinish();
        exec->GetOutputs(&node_io_map);
      }
    } else if (e->GetDispatchType() == DispatchType::DP_PEER) {
      SERVING_ENFORCE(opts_.channels->size() == 1,
                      errors::ErrorCode::UNEXPECTED_ERROR);
      auto iter = opts_.channels->begin();
      auto exec = BuildRemoteExecute(std::move(ctx), iter->first, iter->second);
      exec->Run();
      exec->WaitToFinish();
      exec->GetOutputs(&node_io_map);
    } else {
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "unsupported dispatch type: {}",
                    DispatchType_Name(e->GetDispatchType()));
    }
  }

  auto iter = node_io_map.find(exit_node_name);
  SERVING_ENFORCE(iter != node_io_map.end(),
                  errors::ErrorCode::UNEXPECTED_ERROR);
  DealFinalResult(iter->second, request, opts_.party_id, response);
}

std::unique_ptr<RemoteExecute> Predictor::BuildRemoteExecute(
    ExecuteContext ctx, const std::string& target_id,
    const std::unique_ptr<::google::protobuf::RpcChannel>& channel) {
  return std::make_unique<RemoteExecute>(std::move(ctx), target_id,
                                         channel.get());
}

}  // namespace secretflow::serving
