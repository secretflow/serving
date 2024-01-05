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

#include "secretflow_serving/feature_adapter/file_adapter.h"

#include "arrow/compute/api.h"
#include "arrow/csv/api.h"
#include "arrow/io/api.h"

#include "secretflow_serving/feature_adapter/feature_adapter_factory.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::feature {

FileAdapter::FileAdapter(
    const FeatureSourceConfig& spec, const std::string& service_id,
    const std::string& party_id,
    const std::shared_ptr<const arrow::Schema>& feature_schema)
    : FeatureAdapter(spec, service_id, party_id, feature_schema) {
  SERVING_ENFORCE(spec_.has_csv_opts(), errors::ErrorCode::INVALID_ARGUMENT,
                  "invalid mock options");

  id_name_ = spec_.csv_opts().id_name();

  // read csv file
  std::shared_ptr<arrow::io::ReadableFile> file;
  SERVING_GET_ARROW_RESULT(
      arrow::io::ReadableFile::Open(spec_.csv_opts().file_path()), file);

  arrow::csv::ConvertOptions convert_options;
  // id field are always seen as string
  convert_options.include_columns.push_back(spec_.csv_opts().id_name());
  convert_options.column_types[spec_.csv_opts().id_name()] = arrow::utf8();
  // schemas are discribed by feature_schema_
  for (int i = 0; i < feature_schema_->num_fields(); ++i) {
    std::shared_ptr<arrow::Field> field = feature_schema_->field(i);
    convert_options.include_columns.push_back(field->name());
    convert_options.column_types[field->name()] = field->type();
  }
  std::shared_ptr<arrow::csv::TableReader> csv_reader;
  SERVING_GET_ARROW_RESULT(
      arrow::csv::TableReader::Make(arrow::io::default_io_context(), file,
                                    arrow::csv::ReadOptions::Defaults(),
                                    arrow::csv::ParseOptions::Defaults(),
                                    convert_options),
      csv_reader);

  // Memory usage: TableReader will load all of the data into memory at once
  SERVING_GET_ARROW_RESULT(csv_reader->Read(), csv_table_);
}

void FileAdapter::OnFetchFeature(const Request& request, Response* response) {
  // query data is unique
  const auto& query_datas = request.fs_param->query_datas();
  std::set<std::string> query_data_set(query_datas.begin(), query_datas.end());
  SERVING_ENFORCE((int)query_data_set.size() == query_datas.size(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "query datas in request are duplicated.");

  arrow::StringBuilder builder;
  std::vector<std::string> query_data_vec(
      request.fs_param->query_datas().begin(),
      request.fs_param->query_datas().end());
  SERVING_CHECK_ARROW_STATUS(builder.AppendValues(std::move(query_data_vec)));
  std::shared_ptr<arrow::Array> query_data_array;
  SERVING_CHECK_ARROW_STATUS(builder.Finish(&query_data_array));

  // query datas is a subset of id column
  const auto& id_column = csv_table_->GetColumnByName(id_name_);
  {
    arrow::Datum is_in;
    SERVING_GET_ARROW_RESULT(arrow::compute::IsIn(query_data_array, id_column),
                             is_in);
    const auto is_in_array =
        std::static_pointer_cast<arrow::BooleanArray>(is_in.make_array());
    SERVING_ENFORCE(is_in_array->true_count() == is_in_array->length(),
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "query data is not in csv file.");
  }

  // filter query datas
  arrow::Datum filter;
  SERVING_GET_ARROW_RESULT(
      arrow::compute::IsIn(csv_table_->GetColumnByName(id_name_),
                           query_data_array),
      filter);
  arrow::Datum filtered_table;
  SERVING_GET_ARROW_RESULT(arrow::compute::Filter(csv_table_, filter),
                           filtered_table);

  std::shared_ptr<arrow::Table> features;
  // remove id column
  SERVING_GET_ARROW_RESULT(filtered_table.table()->RemoveColumn(0), features);
  SERVING_GET_ARROW_RESULT(features->CombineChunksToBatch(),
                           response->features);
}

REGISTER_ADAPTER(FeatureSourceConfig::OptionsCase::kCsvOpts, FileAdapter);

}  // namespace secretflow::serving::feature
