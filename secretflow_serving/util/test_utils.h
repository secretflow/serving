#pragma once

#include "arrow/api.h"

namespace secretflow::serving::test {

std::shared_ptr<arrow::RecordBatch> ShuffleRecordBatch(
    std::shared_ptr<arrow::RecordBatch> input_batch);

}  // namespace secretflow::serving::test
