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

#include "secretflow_serving/server/execution_core.h"

#include "spdlog/spdlog.h"
#include "yacl/utils/elapsed_timer.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/thread_pool.h"

namespace secretflow::serving {

ExecutionCore::ExecutionCore(Options opts)
    : opts_(std::move(opts)),
      stats_({{"handler", "ExecutionCore"}, {"party_id", opts_.party_id}}) {
  SERVING_ENFORCE(!opts_.id.empty(), errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(!opts_.party_id.empty(), errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(opts_.executable, errors::ErrorCode::INVALID_ARGUMENT);
  SERVING_ENFORCE(opts_.op_exec_workers_num > 0,
                  errors::ErrorCode::INVALID_ARGUMENT);

  ThreadPool::GetInstance()->Start(opts_.op_exec_workers_num);

  // key: model input feature name
  // value: source or predefined feature name
  std::unordered_map<std::string, std::string> model_feature_mapping;
  valid_feature_mapping_flag_ = false;
  if (opts_.feature_mapping.has_value()) {
    for (const auto& pair : opts_.feature_mapping.value()) {
      if (pair.first != pair.second) {
        valid_feature_mapping_flag_ = true;
      }
      SERVING_ENFORCE(
          model_feature_mapping.emplace(pair.second, pair.first).second,
          errors::ErrorCode::INVALID_ARGUMENT,
          "found duplicate feature mapping value:{}", pair.second);
    }
  }

  const auto& model_input_schema = opts_.executable->GetInputFeatureSchema();
  if (model_feature_mapping.empty()) {
    source_schema_ = model_input_schema;
  } else {
    arrow::SchemaBuilder builder;
    int num_fields = model_input_schema->num_fields();
    for (int i = 0; i < num_fields; ++i) {
      const auto& f = model_input_schema->field(i);
      auto iter = model_feature_mapping.find(f->name());
      SERVING_ENFORCE(iter != model_feature_mapping.end(),
                      errors::ErrorCode::INVALID_ARGUMENT,
                      "can not found {} in feature mapping rule", f->name());
      SERVING_CHECK_ARROW_STATUS(
          builder.AddField(arrow::field(iter->second, f->type())));
    }
    SERVING_GET_ARROW_RESULT(builder.Finish(), source_schema_);
  }

  if (opts_.feature_source_config.has_value()) {
    SPDLOG_INFO("create feature adapter, type:{}",
                static_cast<int>(opts_.feature_source_config->options_case()));
    feature_adapter_ = feature::FeatureAdapterFactory::GetInstance()->Create(
        *opts_.feature_source_config, opts_.id, opts_.party_id, source_schema_);
  }
}

void ExecutionCore::Execute(const apis::ExecuteRequest* request,
                            apis::ExecuteResponse* response) {
  SPDLOG_DEBUG("execute core begin, request: {}", request->ShortDebugString());
  yacl::ElapsedTimer timer;
  try {
    SERVING_ENFORCE(request->service_spec().id() == opts_.id,
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "invalid service spec id: {}",
                    request->service_spec().id());
    response->mutable_service_spec()->CopyFrom(request->service_spec());

    std::shared_ptr<arrow::RecordBatch> features;
    if (request->feature_source().type() ==
        apis::FeatureSourceType::FS_SERVICE) {
      SERVING_ENFORCE(
          !request->feature_source().fs_param().query_datas().empty(),
          errors::ErrorCode::INVALID_ARGUMENT,
          "get empty feature service query datas.");
      SERVING_ENFORCE(request->task().nodes().empty(),
                      errors::ErrorCode::LOGIC_ERROR);
      features = BatchFetchFeatures(request, response);
    } else if (request->feature_source().type() ==
               apis::FeatureSourceType::FS_PREDEFINED) {
      SERVING_ENFORCE(!request->feature_source().predefineds().empty(),
                      errors::ErrorCode::INVALID_ARGUMENT,
                      "get empty predefined features.");
      SERVING_ENFORCE(request->task().nodes().empty(),
                      errors::ErrorCode::LOGIC_ERROR);
      features = FeaturesToRecordBatch(request->feature_source().predefineds(),
                                       source_schema_);
    }
    features = ApplyFeatureMappingRule(features);

    // executable run
    Executable::Task task;
    task.id = request->task().execution_id();
    task.features = features;
    task.node_inputs = std::make_shared<std::unordered_map<
        std::string, std::shared_ptr<op::OpComputeInputs>>>();
    for (const auto& n : request->task().nodes()) {
      auto compute_inputs = std::make_shared<op::OpComputeInputs>();
      for (const auto& io : n.ios()) {
        std::vector<std::shared_ptr<arrow::RecordBatch>> inputs;
        for (const auto& d : io.datas()) {
          inputs.emplace_back(DeserializeRecordBatch(d));
        }
        compute_inputs->emplace_back(std::move(inputs));
      }
      task.node_inputs->emplace(n.name(), std::move(compute_inputs));
    }
    opts_.executable->Run(task);

    for (auto& output : *task.outputs) {
      auto* node_io = response->mutable_result()->add_nodes();
      node_io->set_name(std::move(output.node_name));
      auto* io_data = node_io->add_ios();
      io_data->add_datas(SerializeRecordBatch(output.table));
    }
    response->mutable_status()->set_code(errors::ErrorCode::OK);
  } catch (const Exception& e) {
    SPDLOG_ERROR("execute failed, code:{}, msg:{}, stack:{}", e.code(),
                 e.what(), e.stack_trace());
    response->mutable_status()->set_code(e.code());
    response->mutable_status()->set_msg(e.what());
  } catch (const std::exception& e) {
    SPDLOG_ERROR("execute failed, msg:{}", e.what());
    response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
    response->mutable_status()->set_msg(e.what());
  }
  timer.Pause();

  RecordMetrics(*request, *response, timer.CountMs(), "Execute");

  SPDLOG_DEBUG("execute end, response: {}", response->ShortDebugString());
}

void ExecutionCore::RecordMetrics(const apis::ExecuteRequest& request,
                                  const apis::ExecuteResponse& response,
                                  double duration_ms,
                                  const std::string& action) {
  stats_.execute_request_counter_family
      .Add(::prometheus::Labels(
          {{"service_id", request.service_spec().id()},
           {"action", action},
           {"code", std::to_string(response.status().code())},
           {"requester_id", request.requester_id()},
           {"feature_source_type",
            FeatureSourceType_Name(request.feature_source().type())}}))
      .Increment();
  stats_.execute_request_duration_summary_family
      .Add(::prometheus::Labels({{"service_id", request.service_spec().id()},
                                 {"action", action}}),
           ::prometheus::Summary::Quantiles(
               {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))
      .Observe(duration_ms);
}

std::shared_ptr<arrow::RecordBatch> ExecutionCore::BatchFetchFeatures(
    const apis::ExecuteRequest* request,
    apis::ExecuteResponse* response) const {
  SERVING_ENFORCE(feature_adapter_, errors::ErrorCode::INVALID_ARGUMENT,
                  "feature source is not set, please check config.");

  yacl::ElapsedTimer timer;
  try {
    feature::FeatureAdapter::Request fa_request;
    fa_request.header = &request->header();
    fa_request.fs_param = &request->feature_source().fs_param();
    feature::FeatureAdapter::Response fa_response;
    fa_response.header = response->mutable_header();
    SPDLOG_INFO("spi begin, request: {}",
            fa_request.fs_param->ShortDebugString());
    feature_adapter_->FetchFeature(fa_request, &fa_response);
    timer.Pause();
    SPDLOG_INFO("spi end, response: {} \ntime: {}",
            fa_response.features->ToString(), timer.CountMs());
    RecordBatchFeatureMetrics(request->service_spec().id(),
                              request->requester_id(), errors::ErrorCode::OK,
                              timer.CountMs());
    return fa_response.features;
  } catch (Exception& e) {
    RecordBatchFeatureMetrics(request->service_spec().id(),
                              request->requester_id(), e.code(),
                              timer.CountMs());
    throw e;
  }
}

std::shared_ptr<arrow::RecordBatch> ExecutionCore::ApplyFeatureMappingRule(
    const std::shared_ptr<arrow::RecordBatch>& features) {
  if (features == nullptr || !valid_feature_mapping_flag_) {
    // no need mapping
    return features;
  }
  const auto& feature_mapping = opts_.feature_mapping.value();

  int num_cols = features->num_columns();
  const auto& old_schema = features->schema();
  arrow::SchemaBuilder builder;
  for (int i = 0; i < num_cols; ++i) {
    auto field = old_schema->field(i);
    auto iter = feature_mapping.find(field->name());
    if (iter != feature_mapping.end()) {
      field = arrow::field(iter->second, field->type());
    }
    SERVING_CHECK_ARROW_STATUS(builder.AddField(field));
  }

  std::shared_ptr<arrow::Schema> schema;
  SERVING_GET_ARROW_RESULT(builder.Finish(), schema);

  return MakeRecordBatch(schema, features->num_rows(), features->columns());
}

void ExecutionCore::RecordBatchFeatureMetrics(const std::string& service_id,
                                              const std::string& requester_id,
                                              int code,
                                              double duration_ms) const {
  stats_.fetch_feature_counter_family
      .Add(::prometheus::Labels({{"service_id", service_id},
                                 {"action", "FetchFeature"},
                                 {"code", std::to_string(code)},
                                 {"requester_id", requester_id}}))
      .Increment();
  stats_.fetch_feature_duration_summary_family
      .Add(::prometheus::Labels(
               {{"service_id", service_id}, {"action", "FetchFeature"}}),
           ::prometheus::Summary::Quantiles(
               {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}))
      .Observe(duration_ms);
}

ExecutionCore::Stats::Stats(
    std::map<std::string, std::string> labels,
    const std::shared_ptr<::prometheus::Registry>& registry)
    : execute_request_counter_family(
          ::prometheus::BuildCounter()
              .Name("execution_core_request_count")
              .Help("How many execution requests are handled by "
                    "this ExecutionCore.")
              .Labels(labels)
              .Register(*registry)),
      execute_request_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("execution_core_request_duration_milliseconds")
              .Help("ExecutionCore api request duration in milliseconds")
              .Labels(labels)
              .Register(*registry)),
      fetch_feature_counter_family(
          ::prometheus::BuildCounter()
              .Name("fetch_feature_counter")
              .Help("How many times to fetch remote features service by "
                    "this ExecutionCore.")
              .Labels(labels)
              .Register(*registry)),
      fetch_feature_duration_summary_family(
          ::prometheus::BuildSummary()
              .Name("fetch_feature_duration_milliseconds")
              .Help("durations of fetching remote features in milliseconds")
              .Labels(labels)
              .Register(*registry)) {}

}  // namespace secretflow::serving
