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

#include "secretflow_serving/util/csv_extractor.h"

#include <arrow/io/api.h>
#include <arrow/ipc/api.h>
#include <linux/limits.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "arrow/compute/api.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace {

std::shared_ptr<arrow::Schema> GetSchemaFromFeatureFields(
    const ::google::protobuf::RepeatedPtrField<FeatureField> &feature_fields) {
  std::vector<std::shared_ptr<arrow::Field>> fields;
  for (const auto &field : feature_fields) {
    fields.emplace_back(
        arrow::field(field.name(), FieldTypeToDataType(field.type())));
  }

  return arrow::schema(fields);
}

std::vector<size_t> GetRowsOrder(
    const std::unordered_map<std::string, size_t> &id_column,
    const std::vector<std::string> &ids) {
  std::vector<size_t> id_fetch_order;
  id_fetch_order.resize(ids.size());
  std::iota(id_fetch_order.begin(), id_fetch_order.end(), 0);
  std::sort(id_fetch_order.begin(), id_fetch_order.end(),
            [&](size_t lhs, size_t rhs) -> bool {
              return id_column.at(ids[lhs]) < id_column.at(ids[rhs]);
            });

  // ith fetched row should be put to position id_fetch_order[i] of
  // feature_values array, so id_fetch_order trans csv_row_index to
  // id_index(feature_values_index), but we want id_index(feature_values_index)
  // to csv_row_index then we can fill feature_values with csv_row_index_th
  // element of csv_row
  std::vector<size_t> row_order(id_fetch_order.size());
  for (size_t i = 0; i < id_fetch_order.size(); ++i) {
    row_order[id_fetch_order[i]] = i;
  }

  return row_order;
}

class ArrayReorderVisitor : public arrow::ArrayVisitor {
 public:
  explicit ArrayReorderVisitor(const std::vector<size_t> &order)
      : order_(&order) {}

  arrow::Status Visit(const arrow::BooleanArray &array) override {
    return VisitHelper<arrow::BooleanBuilder>(array);
  }
  arrow::Status Visit(const arrow::Int8Array &array) override {
    return VisitHelper<arrow::Int8Builder>(array);
  }
  arrow::Status Visit(const arrow::UInt8Array &array) override {
    return VisitHelper<arrow::UInt8Builder>(array);
  }
  arrow::Status Visit(const arrow::Int16Array &array) override {
    return VisitHelper<arrow::Int16Builder>(array);
  }
  arrow::Status Visit(const arrow::UInt16Array &array) override {
    return VisitHelper<arrow::UInt16Builder>(array);
  }
  arrow::Status Visit(const arrow::Int32Array &array) override {
    return VisitHelper<arrow::Int32Builder>(array);
  }
  arrow::Status Visit(const arrow::Int64Array &array) override {
    return VisitHelper<arrow::Int64Builder>(array);
  }
  arrow::Status Visit(const arrow::FloatArray &array) override {
    return VisitHelper<arrow::FloatBuilder>(array);
  }
  arrow::Status Visit(const arrow::DoubleArray &array) override {
    return VisitHelper<arrow::DoubleBuilder>(array);
  }
  arrow::Status Visit(const arrow::StringArray &array) override {
    return VisitHelper<arrow::StringBuilder>(array);
  }

  std::shared_ptr<arrow::Array> GetResult() { return result_; }

 private:
  template <typename Builder, typename Array>
  arrow::Status VisitHelper(const Array &array) {
    const auto &order_index = *order_;
    Builder builder;
    for (int64_t i = 0; i != array.length(); ++i) {
      SERVING_CHECK_ARROW_STATUS(builder.Append(array.Value(order_index[i])));
    }
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&result_));
    return arrow::Status::OK();
  }

  const std::vector<size_t> *order_{nullptr};
  std::shared_ptr<arrow::Array> result_;
};

std::shared_ptr<arrow::RecordBatch> ReorderRows(
    std::shared_ptr<arrow::RecordBatch> rows,
    const std::vector<size_t> &order) {
  const auto &raw_columns = rows->columns();
  std::vector<std::shared_ptr<arrow::Array>> reordered_rows;
  reordered_rows.reserve(raw_columns.size());
  for (const auto &raw_col : raw_columns) {
    auto visitor = ArrayReorderVisitor(order);
    SERVING_CHECK_ARROW_STATUS(raw_col->Accept(&visitor));
    reordered_rows.push_back(visitor.GetResult());
  }
  return arrow::RecordBatch::Make(rows->schema(), rows->num_rows(),
                                  reordered_rows);
}

std::vector<std::string> GetIdsFromQueryDatas(
    const ::google::protobuf::RepeatedPtrField<std::string> &query_data) {
  std::vector<std::string> ids;
  for (auto &data : query_data) {
    ids.push_back(data);
  }
  return ids;
}

}  // namespace

CSVExtractor::CSVExtractor(const std::shared_ptr<const arrow::Schema> &schema,
                           std::string filename, std::string id_column_name)
    : CSVExtractor(filename, id_column_name) {
  FetchTable(schema);
}

std::shared_ptr<arrow::Table> CSVExtractor::FetchTable(
    const std::shared_ptr<const arrow::Schema> &schema) {
  auto schema_str = schema->ToString();
  if (schema_tables_cache_.find(schema_str) == schema_tables_cache_.end()) {
    auto table = ReadCsvFileToTable(filename_, schema);
    schema_tables_cache_[schema->ToString()] = table;
    return table;
  } else {
    return schema_tables_cache_[schema->ToString()];
  }
}

std::shared_ptr<arrow::Table> CSVExtractor::FetchTable(
    const ::google::protobuf::RepeatedPtrField<FeatureField> &fields) {
  auto schema = GetSchemaFromFeatureFields(fields);
  return FetchTable(schema);
}

CSVExtractor::CSVExtractor(std::string filename, std::string id_column_name)
    : filename_(std::move(filename)), id_name_(std::move(id_column_name)) {
  SPDLOG_INFO("init CSVExtractor with file: {}, id_name: {}", filename_,
              id_name_);
  id_column_ = GetIdColumnFromFile(filename_, id_name_);
  SPDLOG_INFO("read csv file success, id_column length: {}",
              id_column_->length());
  std::shared_ptr<arrow::Scalar> scalar;
  for (int64_t i = 0; i < id_column_->length(); ++i) {
    SERVING_GET_ARROW_RESULT(id_column_->GetScalar(i), scalar);

    id_index_map_[std::string(std::string_view(
        *std::static_pointer_cast<arrow::StringScalar>(scalar)->value))] = i;
  }
}

std::shared_ptr<arrow::RecordBatch> CSVExtractor::ExtractRows(
    const std::shared_ptr<const arrow::Schema> &schema,
    const ::google::protobuf::RepeatedPtrField<std::string> &query_data) {
  auto ids = GetIdsFromQueryDatas(query_data);
  return ExtractRows(schema, ids);
}

std::shared_ptr<arrow::RecordBatch> CSVExtractor::ExtractRows(
    const ::google::protobuf::RepeatedPtrField<FeatureField> &fields,
    const std::vector<std::string> &ids) {
  auto schema = GetSchemaFromFeatureFields(fields);

  return ExtractRows(schema, ids);
}

std::shared_ptr<arrow::RecordBatch> CSVExtractor::ExtractRows(
    const ::google::protobuf::RepeatedPtrField<FeatureField> &fields,
    const ::google::protobuf::RepeatedPtrField<std::string> &query_data) {
  auto schema = GetSchemaFromFeatureFields(fields);
  auto ids = GetIdsFromQueryDatas(query_data);
  return ExtractRows(schema, ids);
}

std::shared_ptr<arrow::RecordBatch> CSVExtractor::ExtractRows(
    const std::shared_ptr<const arrow::Schema> &schema,
    const std::vector<std::string> &ids) {
  // query data is unique
  std::set<std::string> id_set(ids.begin(), ids.end());
  SERVING_ENFORCE(id_set.size() == ids.size(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "query datas in request are duplicated.");

  SPDLOG_DEBUG("Get request schema: {}", schema->ToString());
  SPDLOG_DEBUG("request ids: {} ", fmt::join(ids, ", "));

  std::shared_ptr<arrow::Table> table = FetchTable(schema);

  auto filter = GetRowsFilter(id_column_, ids);
  auto order = GetRowsOrder(id_index_map_, ids);
  SPDLOG_DEBUG("row order: {} ", fmt::join(order, ", "));

  auto rows = ExtractRowsFromTable(table, filter);
  SPDLOG_DEBUG("extract {} rows", rows->num_rows());
  auto reordered_rows = ReorderRows(rows, order);
  return reordered_rows;
}

}  // namespace secretflow::serving
