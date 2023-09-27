// Copyright (c) 2020 Ant Financial. All rights reserved.

#pragma once

#include <arrow/api.h>
#include <arrow/dataset/api.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "sqlite3.h"

namespace secretflow::serving::ops::sql {

class SqlProcessor {
  struct sqlite3_deleter {
    void operator()(sqlite3* sql) { sqlite3_close_v2(sql); }
  };

 public:
  SqlProcessor();

  std::unordered_map<std::string, std::shared_ptr<arrow::Array>> GetSqlResult(
      const std::string& sql, const std::string& table_name,
      const std::vector<std::string>& feature_names,
      const std::vector<std::string>& feature_types,
      const arrow::RecordBatch& batch) const;

 private:
  void CreateTable(const std::string& table_name,
                   const std::vector<std::string>& feature_names,
                   const std::vector<std::string>& feature_types) const;

  void LoadTableData(const std::string& table_name,
                     const std::vector<std::string>& feature_names,
                     const std::vector<std::string>& feature_types,
                     const arrow::RecordBatch& batch) const;

  void RunSql(const std::string& sql,
              std::unordered_map<std::string, std::shared_ptr<arrow::Array>>*
                  result_map) const;

 private:
  std::unique_ptr<sqlite3, sqlite3_deleter> db_;
  std::string file_;
};

}  // namespace secretflow::serving::ops::sql