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

#include "secretflow_serving/util/test_utils.h"

#include <random>

namespace secretflow::serving::test {

std::shared_ptr<arrow::RecordBatch> ShuffleRecordBatch(
    std::shared_ptr<arrow::RecordBatch> input_batch) {
  auto fields = input_batch->schema()->fields();

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(fields.begin(), fields.end(), g);

  std::vector<std::shared_ptr<arrow::Array>> new_columns;
  new_columns.reserve(fields.size());
  for (const auto& f : fields) {
    new_columns.emplace_back(
        input_batch->column(input_batch->schema()->GetFieldIndex(f->name())));
  }

  return arrow::RecordBatch::Make(arrow::schema(fields),
                                  input_batch->num_rows(), new_columns);
}

}  // namespace secretflow::serving::test
