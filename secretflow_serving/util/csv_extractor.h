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

#pragma once

#include <utility>

#include "arrow/api.h"
#include "arrow/csv/api.h"
#include "arrow/io/api.h"
#include "google/protobuf/repeated_field.h"

#include "secretflow_serving/spis/batch_feature_service.pb.h"

namespace secretflow::serving {

class CSVExtractor {
 public:
  CSVExtractor(std::string filename, std::string id_column_name);
  CSVExtractor(const std::shared_ptr<const arrow::Schema> &schema,
               std::string filename, std::string id_column_name);

  std::shared_ptr<arrow::Table> FetchTable(
      const std::shared_ptr<const arrow::Schema> &schema);
  std::shared_ptr<arrow::Table> FetchTable(
      const ::google::protobuf::RepeatedPtrField<FeatureField> &fields);

  std::shared_ptr<arrow::RecordBatch> ExtractRows(
      const ::google::protobuf::RepeatedPtrField<FeatureField> &fields,
      const ::google::protobuf::RepeatedPtrField<std::string> &query_data);

  std::shared_ptr<arrow::RecordBatch> ExtractRows(
      const std::shared_ptr<const arrow::Schema> &schema,
      const ::google::protobuf::RepeatedPtrField<std::string> &query_data);

  std::shared_ptr<arrow::RecordBatch> ExtractRows(
      const std::shared_ptr<const arrow::Schema> &schema,
      const std::vector<std::string> &ids);

  std::shared_ptr<arrow::RecordBatch> ExtractRows(
      const ::google::protobuf::RepeatedPtrField<FeatureField> &fields,
      const std::vector<std::string> &ids);

 private:
  std::string filename_;
  std::string id_name_;
  std::shared_ptr<arrow::ChunkedArray> id_column_;
  std::unordered_map<std::string, size_t> id_index_map_;

  std::map<std::string, std::shared_ptr<arrow::Table>> schema_tables_cache_;
};

}  // namespace secretflow::serving
