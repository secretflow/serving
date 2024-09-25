// Copyright 2024 Ant Group Co., Ltd.
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

#include "secretflow_serving/tools/inferencer/inference_executor.h"

#include <fstream>

#include "spdlog/spdlog.h"

#include "secretflow_serving/tools/inferencer/control_service_impl.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/csv_util.h"
#include "secretflow_serving/util/network.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/tools/inferencer/inference_service.pb.h"

namespace secretflow::serving::tools {

namespace {

const size_t kInitCheckRetryCount = 60;
const size_t kInitCheckRetryIntervalMs = 1000;

const size_t kSendRetryCount = 30;
const size_t kSendRetryIntervalMs = 1000;

const size_t kHeartBeatCheckIntervalMs = 3000;
const size_t kHeartBeatCheckFailedNum = 10;

}  // namespace

namespace {
int32_t GetCsvFileRowNum(const std::string& file_path) {
  std::ifstream file(file_path);
  SERVING_ENFORCE(file.is_open(), errors::ErrorCode::IO_ERROR, "open {} failed",
                  file_path);

  std::string line;
  // skip header
  std::getline(file, line);

  int32_t row_num = 0;
  while (std::getline(file, line)) {
    row_num++;
  }
  file.close();

  return row_num;
}
}  // namespace

InferenceExecutor::InferenceExecutor(Options opts) : opts_(std::move(opts)) {
  // TODO: check config valiable
  SERVING_ENFORCE(opts_.serving_conf.has_feature_source_conf(),
                  errors::INVALID_ARGUMENT);
  SERVING_ENFORCE(opts_.serving_conf.feature_source_conf().has_streaming_opts(),
                  errors::INVALID_ARGUMENT);

  row_num_ = GetCsvFileRowNum(
      opts_.serving_conf.feature_source_conf().streaming_opts().file_path());

  channels_ = BuildChannelsFromConfig(opts_.serving_conf.cluster_conf());

  // begin services
  Server::Options server_opts{
      .service_id = opts_.serving_conf.id(),
      .server_config = opts_.serving_conf.server_conf(),
      .cluster_config = opts_.serving_conf.cluster_conf(),
      .model_config = opts_.serving_conf.model_conf(),
      .feature_source_config = opts_.serving_conf.feature_source_conf()};
  server_ = std::make_unique<Server>(std::move(server_opts));

  cntl_svc_ = std::make_unique<InferenceControlServiceImpl>(
      opts_.inference_conf.requester_id(), row_num_);
  server_->Start(channels_, cntl_svc_.get());

  cntl_svc_->ReadyToServe();

  prediction_core_ = server_->GetPredictionCore();
}

InferenceExecutor::~InferenceExecutor() {
  StopKeepAlive();
  prediction_core_ = nullptr;
}

void InferenceExecutor::Run() {
  try {
    OnRun();
  } catch (...) {
    stop_flag_ = true;
    throw;
  }
}

void InferenceExecutor::OnRun() {
  SPDLOG_INFO("begin batch predict.");

  if (opts_.inference_conf.requester_id() !=
      opts_.serving_conf.cluster_conf().self_id()) {
    SPDLOG_INFO("begin waiting requester init....");

    RetryRunner runner(kInitCheckRetryCount, kInitCheckRetryIntervalMs);
    SERVING_ENFORCE(runner.Run([this]() {
      return cntl_svc_->init_flag() || cntl_svc_->stop_flag();
    }),
                    serving::errors::UNEXPECTED_ERROR,
                    "waiting init msg from {} timeout",
                    opts_.inference_conf.requester_id());

    SPDLOG_INFO("init finish.");

    if (cntl_svc_->stop_flag()) {
      SPDLOG_INFO("stop flag is true, just stop.");
      return;
    }
    WaitForEnd();
    return;
  }

  // init other party
  RetryRunner runner(kSendRetryCount, kSendRetryIntervalMs);
  std::vector<int32_t> row_num_list;
  for (const auto& [p, c] : *channels_) {
    ControlResponse res;
    SERVING_ENFORCE(runner.Run(
                        [this](const std::string& party_id,
                               ::google::protobuf::RpcChannel* channel,
                               ControlResponse* response) {
                          return SendMsg(party_id, channel,
                                         ControlMessageType::CM_INIT, response);
                        },
                        p, c.get(), &res),
                    serving::errors::UNEXPECTED_ERROR,
                    "send init msg to {} failed.", p);
    row_num_list.emplace_back(res.init_msg().row_num());
  }
  SERVING_ENFORCE(std::all_of(row_num_list.begin(), row_num_list.end(),
                              [this](auto e) { return e == row_num_; }),
                  errors::UNEXPECTED_ERROR,
                  "The number of input file lines of different participants "
                  "does not match. {} vs {}",
                  row_num_,
                  fmt::join(row_num_list.begin(), row_num_list.end(), ","));

  // start keepalive
  keepalive_thread_ = std::thread(&InferenceExecutor::KeepAlive, this);

  // build read colums types
  std::unordered_map<std::string, std::shared_ptr<arrow::DataType>> col_types{
      {opts_.serving_conf.feature_source_conf().streaming_opts().id_name(),
       arrow::utf8()}};
  for (const auto& c : opts_.inference_conf.additional_col_names()) {
    col_types.emplace(c, arrow::utf8());
  }

  // build output schema
  std::shared_ptr<arrow::Schema> output_schema;
  {
    std::vector<std::shared_ptr<arrow::Field>> fields{
        arrow::field(opts_.inference_conf.score_col_name(), arrow::float64())};
    std::transform(
        col_types.begin(), col_types.end(), std::back_inserter(fields),
        [](const auto& p) { return arrow::field(p.first, arrow::utf8()); });
    output_schema = arrow::schema(std::move(fields));
  }

  // csv reader
  arrow::csv::ReadOptions read_opts = arrow::csv::ReadOptions::Defaults();
  if (opts_.inference_conf.block_size() > 0) {
    read_opts.block_size = opts_.inference_conf.block_size();
  }
  std::shared_ptr<arrow::csv::StreamingReader> csv_reader =
      csv::BuildStreamingReader(
          opts_.serving_conf.feature_source_conf().streaming_opts().file_path(),
          std::move(col_types), read_opts);

  // build result writer
  std::shared_ptr<arrow::ipc::RecordBatchWriter> csv_writer =
      csv::BuildeStreamingWriter(opts_.inference_conf.result_file_path(),
                                 output_schema);

  // begin batch predict
  apis::PredictRequest pred_request;
  pred_request.mutable_service_spec()->set_id(opts_.serving_conf.id());
  auto* fs_params = pred_request.mutable_fs_params();
  fs_params->insert({opts_.serving_conf.cluster_conf().self_id(), {}});
  for (const auto& [p, c] : *channels_) {
    fs_params->insert({p, {}});
  }
  int32_t idx = 0;
  std::shared_ptr<arrow::RecordBatch> batch;
  while (true) {
    ++idx;
    SERVING_CHECK_ARROW_STATUS(csv_reader->ReadNext(&batch));
    if (!batch) {
      // read finish
      break;
    }
    SERVING_ENFORCE_GT(
        batch->num_rows(), 0,
        "may be because `block_size` is configured too small: {}",
        opts_.inference_conf.block_size());

    auto id_array = std::static_pointer_cast<arrow::StringArray>(
        batch->GetColumnByName(opts_.serving_conf.feature_source_conf()
                                   .streaming_opts()
                                   .id_name()));

    // build fs_params
    for (auto& [party, param] : *fs_params) {
      param.clear_query_datas();
      param.set_query_context(std::to_string(idx));
      for (int64_t i = 0; i < id_array->length(); ++i) {
        auto item = id_array->Value(i);
        param.add_query_datas(item.data(), item.length());
      }
    }

    // batch predict
    apis::PredictResponse pred_response;
    prediction_core_->PredictImpl(&pred_request, &pred_response);

    // build score array
    std::shared_ptr<arrow::Array> score_array;
    arrow::DoubleBuilder builder;
    for (const auto& r : pred_response.results()) {
      // TODO: only support one score in result
      SERVING_CHECK_ARROW_STATUS(builder.Append(r.scores(0).value()));
    }
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&score_array));

    // write result
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    arrays.reserve(output_schema->num_fields());
    for (const auto& f : output_schema->fields()) {
      if (f->name() == opts_.inference_conf.score_col_name()) {
        arrays.emplace_back(std::move(score_array));
        continue;
      }
      arrays.emplace_back(batch->GetColumnByName(f->name()));
    }
    auto result_batch = MakeRecordBatch(
        output_schema, pred_response.results_size(), std::move(arrays));
    SERVING_CHECK_ARROW_STATUS(csv_writer->WriteRecordBatch(*result_batch));
  }
  SERVING_CHECK_ARROW_STATUS(csv_writer->Close());

  // send end msg
  for (const auto& [p, c] : *channels_) {
    SERVING_ENFORCE(runner.Run(
                        [this](const std::string& party_id,
                               ::google::protobuf::RpcChannel* channel) {
                          ControlResponse res;
                          return SendMsg(party_id, channel,
                                         ControlMessageType::CM_STOP, &res);
                        },
                        p, c.get()),
                    serving::errors::UNEXPECTED_ERROR,
                    "send stop msg to {} failed.", p);
  }

  SPDLOG_INFO("batch predict finish.");
}

void InferenceExecutor::WaitForEnd() {
  SPDLOG_INFO("waiting for end");
  uint64_t last_heart_beat_count = 0;
  size_t failed_count = 0;
  while (failed_count <= kHeartBeatCheckFailedNum && !cntl_svc_->stop_flag()) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(kHeartBeatCheckIntervalMs));

    auto now = cntl_svc_->heart_beat_count();
    if (now == last_heart_beat_count) {
      ++failed_count;
    } else {
      last_heart_beat_count = now;
      failed_count = 0;
    }
  }
  if (failed_count > kHeartBeatCheckFailedNum) {
    SPDLOG_ERROR("heart beat timeout...");
  }
  SPDLOG_INFO("finish wait.");
}

void InferenceExecutor::KeepAlive() {
  while (true) {
    if (stop_flag_) {
      SPDLOG_INFO("stop send keepalive msg.");
      return;
    }
    for (const auto& [p, c] : *channels_) {
      ControlResponse res;
      SendMsg(p, c.get(), ControlMessageType::CM_KEEPALIVE, &res);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }
}

void InferenceExecutor::StopKeepAlive() {
  stop_flag_ = true;
  if (keepalive_thread_.joinable()) {
    keepalive_thread_.join();
  }
}

bool InferenceExecutor::SendMsg(const std::string& target_id,
                                ::google::protobuf::RpcChannel* channel,
                                ControlMessageType type,
                                ControlResponse* response) {
  brpc::Controller cntl;
  // close brpc retry.
  cntl.set_max_retry(0);

  ControlRequest request;
  request.set_party_id(opts_.serving_conf.cluster_conf().self_id());
  request.set_type(type);

  InferenceControlService_Stub stub(channel);
  stub.Push(&cntl, &request, response, nullptr);
  if (cntl.Failed()) {
    SPDLOG_WARN(
        "call ({}) init control failed, msg:{}, may need "
        "retry",
        target_id, cntl.ErrorText());
    return false;
  } else if (!CheckStatusOk(response->status())) {
    SPDLOG_WARN(
        "call ({}) init control msg failed, msg:{}, may need "
        "retry",
        target_id, response->status().msg());
    return false;
  }
  return true;
}

}  // namespace secretflow::serving::tools
