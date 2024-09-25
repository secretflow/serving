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

#include "arrow/csv/api.h"
#include "arrow/ipc/writer.h"

namespace secretflow::serving::csv {

std::shared_ptr<arrow::csv::StreamingReader> BuildStreamingReader(
    const std::string& path,
    std::unordered_map<std::string, std::shared_ptr<arrow::DataType>> col_types,
    const arrow::csv::ReadOptions& read_opts =
        arrow::csv::ReadOptions::Defaults());

std::shared_ptr<arrow::ipc::RecordBatchWriter> BuildeStreamingWriter(
    const std::string& path, const std::shared_ptr<arrow::Schema>& schema);

std::shared_ptr<arrow::Table> ReadCsvFileToTable(
    const std::string& path,
    const std::shared_ptr<const arrow::Schema>& feature_schema);

std::shared_ptr<arrow::ChunkedArray> GetIdColumnFromFile(
    const std::string& filename, const std::string& id_name);

}  // namespace secretflow::serving::csv
