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

#include "secretflow_serving/ops/sql/sql_processor.h"

#include <unistd.h>

#include <filesystem>
#include <iostream>
#include <random>

#include "absl/strings/ascii.h"
#include "boost/lexical_cast.hpp"
#include "fmt/format.h"
#include "spdlog/spdlog.h"
#include "yacl/utils/scope_guard.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::ops::sql {
namespace {

/*
    填充数据结果
*/
void FillRowResult(
    sqlite3_stmt* stmt, bool first_row,
    std::unordered_map<std::string, std::shared_ptr<arrow::Array>>*
        result_map) {
  SERVING_ENFORCE(stmt, errors::ErrorCode::UNKNOWN,
                  "SQL statement is not prepared.");
  size_t column_count = sqlite3_data_count(stmt);
  if (!first_row) {
    SERVING_ENFORCE(
        column_count == result_map->size(), errors::ErrorCode::UNKNOWN,
        "Query result data length {} is unexpected", result_map->size());
  }
  // Process each column
  for (size_t i = 0; i < column_count; i++) {
    std::string col_name = sqlite3_column_name(stmt, i);
    size_t row_column_type = sqlite3_column_type(stmt, i);
    if (first_row) {
      std::shared_ptr<arrow::Array> array;
      result_map->insert(std::make_pair(col_name, std::move(array)));
    }
    if (row_column_type == SQLITE_INTEGER || row_column_type == SQLITE_FLOAT) {
      std::shared_ptr<arrow::DoubleArray> double_array =
          std::static_pointer_cast<arrow::DoubleArray>((*result_map)[col_name]);
      arrow::DoubleBuilder builder;
      if (!first_row) {
        for (int64_t j = 0; j < double_array->length(); ++j) {
          SERVING_CHECK_ARROW_STATUS(builder.Append(double_array->Value(j)));
        }
      }
      SERVING_CHECK_ARROW_STATUS(
          builder.Append(sqlite3_column_double(stmt, i)));
      std::shared_ptr<arrow::Array> new_array;
      SERVING_CHECK_ARROW_STATUS(builder.Finish(&new_array));
      (*result_map)[col_name] = new_array;
    } else {
      // SQLITE_TEXT | SQLITE_BLOB | SQLITE_NULL
      std::shared_ptr<arrow::StringArray> string_array =
          std::static_pointer_cast<arrow::StringArray>((*result_map)[col_name]);
      arrow::StringBuilder builder;
      if (!first_row) {
        for (int64_t j = 0; j < string_array->length(); ++j) {
          SERVING_CHECK_ARROW_STATUS(
              builder.Append(string_array->GetString(j)));
        }
      }
      SERVING_CHECK_ARROW_STATUS(builder.Append(
          reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))));
      std::shared_ptr<arrow::Array> new_array;
      SERVING_CHECK_ARROW_STATUS(builder.Finish(&new_array));
      (*result_map)[col_name] = new_array;
    }
  }
}

}  // namespace

SqlProcessor::SqlProcessor() {
  auto db_ptr = db_.get();
  // 打开数据库连接
  int rc = sqlite3_open_v2(
      NULL, &db_ptr,
      SQLITE_OPEN_CREATE | SQLITE_OPEN_URI | SQLITE_OPEN_READWRITE, NULL);
  // 数据库连接结果检查
  SERVING_ENFORCE(
      rc == SQLITE_OK, errors::ErrorCode::UNEXPECTED_ERROR,
      "Failed to create sqlite db, error code: {}, error message: {}", rc,
      sqlite3_errmsg(db_ptr));
  db_.reset(db_ptr);
}

// 执行sql并获取结果
std::unordered_map<std::string, std::shared_ptr<arrow::Array>>
SqlProcessor::GetSqlResult(const std::string& sql,
                           const std::string& table_name,
                           const std::vector<std::string>& feature_names,
                           const std::vector<std::string>& feature_types,
                           const arrow::RecordBatch& batch) const {
  std::unordered_map<std::string, std::shared_ptr<arrow::Array>> result_map;
  //   创建数据表
  CreateTable(table_name, feature_names, feature_types);
  //   数据表加载数据
  LoadTableData(table_name, feature_names, feature_types, batch);
  //   执行sql并输出结果数据
  RunSql(sql, &result_map);
  return result_map;
}

void SqlProcessor::CreateTable(
    const std::string& table_name,
    const std::vector<std::string>& feature_names,
    const std::vector<std::string>& feature_types) const {
  // 输入参数检查,特征列于特征类型应当匹配
  SERVING_ENFORCE(
      feature_names.size() == feature_types.size(),
      errors::ErrorCode::INVALID_ARGUMENT,
      fmt::format("feature names size {} not match feature type size {}",
                  feature_names.size(), feature_types.size()));

  size_t feature_num = feature_names.size();
  std::vector<std::string> column_schemas;
  for (size_t i = 0; i < feature_num; i++) {
    SERVING_ENFORCE(!feature_names[i].empty(),
                    errors::ErrorCode::INVALID_ARGUMENT,
                    "Column name should not be empty");
    // 分类型解析各特征数据
    if (feature_types[i] == "double") {
      column_schemas.emplace_back(
          fmt::format("{} {}", feature_names[i], "real"));
    } else if (feature_types[i] == "string") {
      column_schemas.emplace_back(
          fmt::format("{} {}", feature_names[i], "text"));
    } else {
      SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                    "unknow feature type {}", feature_types[i]);
    }
  }
  //   构造创建数据表sql
  std::string sql_stmt =
      fmt::format("CREATE TABLE IF NOT EXISTS {} ( {} );", table_name,
                  fmt::join(column_schemas, ", "));

  // 执行创建数据表
  char* sqlite_errmsg = 0;
  ON_SCOPE_EXIT([&] { sqlite3_free(sqlite_errmsg); });
  int rc = sqlite3_exec(db_.get(), sql_stmt.c_str(), NULL, 0, &sqlite_errmsg);
  SERVING_ENFORCE(rc == SQLITE_OK, errors::ErrorCode::UNKNOWN,
                  "SQLite error code={}, error message={}, sql={}", rc,
                  sqlite_errmsg, sql_stmt);
}

void SqlProcessor::LoadTableData(const std::string& table_name,
                                 const std::vector<std::string>& feature_names,
                                 const std::vector<std::string>& feature_types,
                                 const arrow::RecordBatch& batch) const {
  // 构造插入数据sql
  std::ostringstream batch_insert_sql;
  // Start transaction
  batch_insert_sql << "BEGIN TRANSACTION;";
  std::vector<std::string> batch_values;
  auto rows = batch.num_rows();
  auto cols = batch.num_columns();
  for (int64_t i = 0; i < rows; ++i) {
    std::vector<std::string> values;
    for (int j = 0; j < cols; ++j) {
      auto col = batch.column(j);
      std::shared_ptr<arrow::Scalar> raw_scalar;
      SERVING_GET_ARROW_RESULT(col->GetScalar(i), raw_scalar);
      if (feature_types[j] == "string") {
        values.emplace_back(fmt::format(
            "'{}'",
            std::static_pointer_cast<arrow::StringScalar>(raw_scalar)->view()));
      } else if (feature_types[j] == "double") {
        try {
          values.emplace_back(fmt::format(
              "'{}'", std::static_pointer_cast<arrow::DoubleScalar>(raw_scalar)
                          ->value));
        } catch (const std::exception& e) {
          SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                        fmt::format("SqlProcessor: DOUBLE feature {}, {}",
                                    feature_names[j], e.what()));
        }
      } else {
        SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                      "unknow Schema::type {}", feature_types[j]);
      }
    }
    batch_values.emplace_back(fmt::format("({})", fmt::join(values, ", ")));
  }
  const std::string insert_sql = fmt::format(
      "INSERT INTO {} VALUES {};", table_name, fmt::join(batch_values, ", "));
  batch_insert_sql << insert_sql;
  // End transaction
  batch_insert_sql << "END TRANSACTION;";
  // 执行数据插入sql
  char* sqlite_errmsg = 0;
  ON_SCOPE_EXIT([&] { sqlite3_free(sqlite_errmsg); });
  int rc = sqlite3_exec(db_.get(), batch_insert_sql.str().c_str(), NULL, 0,
                        &sqlite_errmsg);
  SERVING_ENFORCE(rc == SQLITE_OK, errors::ErrorCode::UNKNOWN,
                  "SQLite error code={}, error message={}", rc, sqlite_errmsg);
}

void SqlProcessor::RunSql(
    const std::string& sql,
    std::unordered_map<std::string, std::shared_ptr<arrow::Array>>* result_map)
    const {
  sqlite3_stmt* stmt;
  int rc = 0;
  std::string ori_sql = sql;
  absl::StripAsciiWhitespace(&ori_sql);
  if (ori_sql.empty()) {
    SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                  "Sql is empty or whitespace");
    return;
  }
  // add ; to the end
  if (ori_sql.back() != ';') {
    ori_sql += ";";
  }

  // 预编译sql
  rc = sqlite3_prepare_v2(db_.get(), ori_sql.c_str(), strlen(ori_sql.c_str()),
                          &stmt, nullptr);
  ON_SCOPE_EXIT([&] { sqlite3_finalize(stmt); });

  SERVING_ENFORCE(rc == SQLITE_OK, errors::ErrorCode::UNKNOWN,
                  "SQLite error code={}, error message={}, sql={}", rc,
                  sqlite3_errmsg(db_.get()), sql);
  // 执行预编译的sql
  rc = sqlite3_step(stmt);
  bool first_row = true;
  // SQLITE_ROW means current sql query has data to return
  while (rc == SQLITE_ROW) {
    // 填充结果数据
    FillRowResult(stmt, first_row, result_map);
    first_row = false;
    rc = sqlite3_step(stmt);
  }
  SERVING_ENFORCE(rc == SQLITE_DONE, errors::ErrorCode::UNKNOWN,
                  "SQLite error code={}, error message={}", rc,
                  sqlite3_errmsg(db_.get()));
  return;
}

}  // namespace secretflow::serving::ops::sql
