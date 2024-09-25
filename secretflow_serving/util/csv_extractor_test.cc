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

#include <algorithm>
#include <limits>
#include <memory>

#include "arrow/compute/api.h"
#include "butil/files/temp_file.h"
#include "gtest/gtest.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::csv {

TEST(CSVExtractor, TestReadCsvFile) {
  butil::TempFile tmpfile;
  tmpfile.save(1 + R"TEXT(
id,x1,x2,x3,x4
0,0,1.0,alice,dummy
1,1,11.0,bob,dummy
2,2,21.0,carol,dummy
3,3,31.0,dave,dummy
)TEXT");
  CSVExtractor extractor(tmpfile.fname(), "id");
  ::google::protobuf::RepeatedPtrField<FeatureField> fields;
  auto field = fields.Add();
  field->set_name("x1");
  field->set_type(FieldType::FIELD_INT32);

  std::vector<std::string> ids{
      "2",
      "0",
      "3",
      "1",
  };
  auto result = extractor.ExtractRows(fields, ids);
  auto col = result->column(0);
  auto int_col = std::static_pointer_cast<arrow::Int32Array>(col);
  EXPECT_EQ(int_col->length(), ids.size());
  EXPECT_EQ(int_col->Value(0), 2);
  EXPECT_EQ(int_col->Value(1), 0);
  EXPECT_EQ(int_col->Value(2), 3);
  EXPECT_EQ(int_col->Value(3), 1);
}

}  // namespace secretflow::serving::csv
