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

#include "secretflow_serving/util/csv_util.h"

#include "arrow/io/api.h"

#include "secretflow_serving/util/arrow_helper.h"

namespace secretflow::serving::csv {

std::shared_ptr<arrow::csv::StreamingReader> BuildStreamingReader(
    const std::string& path,
    std::unordered_map<std::string, std::shared_ptr<arrow::DataType>> col_types,
    const arrow::csv::ReadOptions& read_opts) {
  std::shared_ptr<arrow::io::ReadableFile> in_file;
  SERVING_GET_ARROW_RESULT(arrow::io::ReadableFile::Open(path), in_file);
  arrow::csv::ConvertOptions convert_options;
  convert_options.column_types = std::move(col_types);
  std::transform(convert_options.column_types.begin(),
                 convert_options.column_types.end(),
                 std::back_inserter(convert_options.include_columns),
                 [](const auto& p) { return p.first; });
  std::shared_ptr<arrow::csv::StreamingReader> csv_reader;
  SERVING_GET_ARROW_RESULT(
      arrow::csv::StreamingReader::Make(
          arrow::io::default_io_context(), in_file, read_opts,
          arrow::csv::ParseOptions::Defaults(), convert_options),
      csv_reader);
  return csv_reader;
}

std::shared_ptr<arrow::ipc::RecordBatchWriter> BuildeStreamingWriter(
    const std::string& path, const std::shared_ptr<arrow::Schema>& schema) {
  std::shared_ptr<arrow::io::FileOutputStream> out_stream;
  SERVING_GET_ARROW_RESULT(arrow::io::FileOutputStream::Open(path), out_stream);
  auto writer_opts = arrow::csv::WriteOptions::Defaults();
  writer_opts.quoting_style = arrow::csv::QuotingStyle::None;
  std::shared_ptr<arrow::ipc::RecordBatchWriter> csv_writer;
  SERVING_GET_ARROW_RESULT(
      arrow::csv::MakeCSVWriter(out_stream, schema, writer_opts), csv_writer);

  return csv_writer;
}

std::shared_ptr<arrow::Table> ReadCsvFileToTable(
    const std::string& path,
    const std::shared_ptr<const arrow::Schema>& feature_schema) {
  // read csv file
  std::shared_ptr<arrow::io::ReadableFile> file;
  SERVING_GET_ARROW_RESULT(arrow::io::ReadableFile::Open(path), file);

  arrow::csv::ConvertOptions convert_options;

  for (int i = 0; i < feature_schema->num_fields(); ++i) {
    std::shared_ptr<arrow::Field> field = feature_schema->field(i);

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

  std::shared_ptr<arrow::Table> table;
  SERVING_GET_ARROW_RESULT(csv_reader->Read(), table);
  return table;
}

std::shared_ptr<arrow::ChunkedArray> GetIdColumnFromFile(
    const std::string& filename, const std::string& id_name) {
  std::vector<std::shared_ptr<arrow::Field>> fields;
  fields.push_back(arrow::field(id_name, arrow::utf8()));
  auto schema = arrow::schema(fields);
  auto table = ReadCsvFileToTable(filename, schema);
  auto id_column = table->GetColumnByName(id_name);
  SERVING_ENFORCE(id_column, errors::ErrorCode::INVALID_ARGUMENT,
                  "column: {} is not in csv file: {}", id_name, filename);

  return id_column;
}

}  // namespace secretflow::serving::csv
