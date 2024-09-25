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

#include "secretflow_serving/feature_adapter/streaming_adapter.h"

#include "gflags/gflags.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/csv_util.h"

DEFINE_bool(inferencer_mode, false,
            "streaming adpater is allowed only if inferencer_mode is true");

namespace secretflow::serving::feature {

namespace {

const char* kUnSetMagicToken = "__UNSET_MAGIC_TOKEN__";

void CheckIdsEqual(const FeatureAdapter::Request& request,
                   const std::shared_ptr<arrow::Array>& id_array) {
  SERVING_ENFORCE_EQ(id_array->length(), request.fs_param->query_datas_size(),
                     "id_array length mismatch with query_datas_size.");
  auto str_array = std::static_pointer_cast<arrow::StringArray>(id_array);
  for (int i = 0; i < request.fs_param->query_datas_size(); ++i) {
    SERVING_ENFORCE_EQ(request.fs_param->query_datas(i), str_array->Value(i));
  }
}
}  // namespace

StreamingAdapter::StreamingAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : FeatureAdapter(spec, service_id, party_id, feature_schema),
      last_context_token_(kUnSetMagicToken) {
  SERVING_ENFORCE(
      FLAGS_inferencer_mode, errors::ErrorCode::LOGIC_ERROR,
      "streaming adpater is allowed only when using the inferencer tool");

  SERVING_ENFORCE(spec_.has_streaming_opts(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "invalid feature source streaming options");
  if (spec_.streaming_opts().file_format().empty()) {
    spec_.mutable_streaming_opts()->set_file_format("CSV");
  }
  SERVING_ENFORCE_EQ(spec_.streaming_opts().file_format(), "CSV");

  std::unordered_map<std::string, std::shared_ptr<arrow::DataType>> col_types;
  for (const auto& f : feature_schema->fields()) {
    col_types.emplace(f->name(), f->type());
  }
  col_types.emplace(spec_.streaming_opts().id_name(), arrow::utf8());

  arrow::csv::ReadOptions read_opts = arrow::csv::ReadOptions::Defaults();
  if (spec_.streaming_opts().block_size() > 0) {
    read_opts.block_size = spec_.streaming_opts().block_size();
  }
  streaming_reader_ = csv::BuildStreamingReader(
      spec_.streaming_opts().file_path(), std::move(col_types), read_opts);
}

void StreamingAdapter::OnFetchFeature(const Request& request,
                                      Response* response) {
  SERVING_ENFORCE_GT(request.fs_param->query_datas_size(), 0);

  std::lock_guard lock(mux_);

  if (!request.fs_param->query_context().empty() &&
      request.fs_param->query_context() == last_context_token_) {
    // The retry request attempts to retrieve previously read data, possibly
    // due to an exception in some other logic.
    response->features = last_batch_;
    CheckIdsEqual(request, response->features->GetColumnByName(
                               spec_.streaming_opts().id_name()));
    return;
  }
  // cleanup cache
  last_batch_ = nullptr;
  last_context_token_ = kUnSetMagicToken;

  int64_t idx = 0;
  std::vector<std::shared_ptr<arrow::RecordBatch>> batches;
  if (cur_batch_) {
    std::shared_ptr<arrow::RecordBatch> slice_batch;
    if (request.fs_param->query_datas_size() <
        cur_batch_->num_rows() - cur_offset_) {
      idx += request.fs_param->query_datas_size();
      slice_batch =
          cur_batch_->Slice(cur_offset_, request.fs_param->query_datas_size());
      cur_offset_ += request.fs_param->query_datas_size();
    } else {
      idx += cur_batch_->num_rows() - cur_offset_;
      slice_batch = cur_batch_->Slice(cur_offset_);
      cur_offset_ = 0;
      cur_batch_ = nullptr;
    }
    batches.emplace_back(std::move(slice_batch));
  }

  std::shared_ptr<arrow::RecordBatch> batch;
  while (request.fs_param->query_datas_size() > idx) {
    SERVING_CHECK_ARROW_STATUS(streaming_reader_->ReadNext(&batch));
    SERVING_ENFORCE(batch, errors::LOGIC_ERROR);
    SERVING_ENFORCE_GT(
        batch->num_rows(), 0,
        "may be because `block_size` is configured too small: {}",
        spec_.streaming_opts().block_size());

    if (request.fs_param->query_datas_size() - idx < batch->num_rows()) {
      auto slice_batch =
          batch->Slice(0, request.fs_param->query_datas_size() - idx);
      cur_batch_ = batch;
      cur_offset_ = slice_batch->num_rows();
      idx += slice_batch->num_rows();
      batches.emplace_back(std::move(slice_batch));
      break;
    } else {
      idx += batch->num_rows();
      cur_batch_ = nullptr;
      cur_offset_ = 0;
      batches.emplace_back(std::move(batch));
    }
  }

  if (batches.size() > 1) {
    std::shared_ptr<arrow::Table> table;
    SERVING_GET_ARROW_RESULT(arrow::Table::FromRecordBatches(batches), table);
    SERVING_GET_ARROW_RESULT(table->CombineChunksToBatch(), response->features);
  } else {
    response->features = std::move(batches[0]);
  }
  CheckIdsEqual(request, response->features->GetColumnByName(
                             spec_.streaming_opts().id_name()));
  // cache current result.
  last_batch_ = response->features;
  last_context_token_ = request.fs_param->query_context();
}

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kStreamingOpts,
                 StreamingAdapter);

}  // namespace secretflow::serving::feature
