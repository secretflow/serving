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

#include "secretflow_serving/ops/arrow_processing.h"

#include <cstdint>
#include <map>

#include "arrow/compute/api.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/ops/node_def_util.h"
#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving::op {

namespace {

const static std::set<std::string> kReturnableFuncNames = {
    compute::ExtendFunctionName_Name(
        compute::ExtendFunctionName::EFN_TB_ADD_COLUMN),
    compute::ExtendFunctionName_Name(
        compute::ExtendFunctionName::EFN_TB_REMOVE_COLUMN),
    compute::ExtendFunctionName_Name(
        compute::ExtendFunctionName::EFN_TB_SET_COLUMN)};

arrow::Datum PbScalar2Datum(const compute::Scalar& scalar) {
  switch (scalar.value_case()) {
    case compute::Scalar::ValueCase::kI8: {
      return arrow::Datum(static_cast<int8_t>(scalar.i8()));
    }
    case compute::Scalar::ValueCase::kUi8: {
      return arrow::Datum(static_cast<uint8_t>(scalar.ui8()));
    }
    case compute::Scalar::ValueCase::kI16: {
      return arrow::Datum(static_cast<int16_t>(scalar.i16()));
    }
    case compute::Scalar::ValueCase::kUi16: {
      return arrow::Datum(static_cast<uint16_t>(scalar.ui16()));
    }
    case compute::Scalar::ValueCase::kI32: {
      return arrow::Datum(scalar.i32());
    }
    case compute::Scalar::ValueCase::kUi32: {
      return arrow::Datum(scalar.ui32());
    }
    case compute::Scalar::ValueCase::kI64: {
      return arrow::Datum(scalar.i64());
    }
    case compute::Scalar::ValueCase::kUi64: {
      return arrow::Datum(scalar.ui64());
    }
    case compute::Scalar::ValueCase::kF: {
      return arrow::Datum(scalar.f());
    }
    case compute::Scalar::ValueCase::kD: {
      return arrow::Datum(scalar.d());
    }
    case compute::Scalar::ValueCase::kS: {
      return arrow::Datum(scalar.s());
    }
    case compute::Scalar::ValueCase::kB: {
      return arrow::Datum(scalar.b());
    }
    default:
      SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                    "invalid pb scalar type: {}",
                    static_cast<int>(scalar.value_case()));
  }
}

std::vector<arrow::Datum::Kind> BuildInputDatumKind(
    const ::google::protobuf::RepeatedPtrField<compute::FunctionInput>&
        func_inputs,
    const std::map<int32_t, arrow::Datum::Kind>& data_id_map) {
  std::vector<arrow::Datum::Kind> results;
  for (const auto& in : func_inputs) {
    if (in.value_case() == compute::FunctionInput::ValueCase::kDataId) {
      auto iter = data_id_map.find(in.data_id());
      SERVING_ENFORCE(iter != data_id_map.end(), errors::ErrorCode::LOGIC_ERROR,
                      "can not found input data_id({})", in.data_id());
      results.emplace_back(iter->second);
    } else if (in.value_case() ==
               compute::FunctionInput::ValueCase::kCustomScalar) {
      results.emplace_back(arrow::Datum::Kind::SCALAR);
    } else {
      SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                    "invalid function input type:{}",
                    static_cast<int>(in.value_case()));
    }
  }

  return results;
}

std::vector<arrow::Datum> BuildInputDatums(
    const ::google::protobuf::RepeatedPtrField<compute::FunctionInput>&
        func_inputs,
    const std::map<int32_t, arrow::Datum>& datas) {
  std::vector<arrow::Datum> results;
  for (const auto& in : func_inputs) {
    if (in.value_case() == compute::FunctionInput::ValueCase::kDataId) {
      auto iter = datas.find(in.data_id());
      SERVING_ENFORCE(iter != datas.end(), errors::ErrorCode::LOGIC_ERROR,
                      "can not found input data_id({})", in.data_id());
      results.emplace_back(iter->second);
    } else if (in.value_case() ==
               compute::FunctionInput::ValueCase::kCustomScalar) {
      results.emplace_back(PbScalar2Datum(in.custom_scalar()));
    } else {
      SERVING_THROW(errors::ErrorCode::INVALID_ARGUMENT,
                    "invalid function input type:{}",
                    static_cast<int>(in.value_case()));
    }
  }

  return results;
}

}  // namespace

ArrowProcessing::ArrowProcessing(OpKernelOptions opts)
    : OpKernel(std::move(opts)) {
  BuildInputSchema();
  BuildOutputSchema();

  // optional attr
  std::string trace_content =
      GetNodeBytesAttr(opts_.node_def, *opts_.op_def, "trace_content");
  if (trace_content.empty()) {
    dummy_flag_ = true;
    return;
  }
  bool content_json_flag =
      GetNodeAttr<bool>(opts_.node_def, *opts_.op_def, "content_json_flag");

  if (content_json_flag) {
    JsonToPb(trace_content, &compute_trace_);
  } else {
    SERVING_ENFORCE(compute_trace_.ParseFromString(trace_content),
                    errors::ErrorCode::DESERIALIZE_FAILED,
                    "parse trace pb bytes failed");
  }

  if (compute_trace_.func_traces().empty()) {
    dummy_flag_ = true;
    return;
  }

  // sanity check
  // check the last compute func is returnable(output record_batch)
  const auto& end_func = *(compute_trace_.func_traces().rbegin());
  SERVING_ENFORCE(
      kReturnableFuncNames.find(end_func.name()) != kReturnableFuncNames.end(),
      errors::ErrorCode::LOGIC_ERROR,
      "the last compute function({}) is not returnable", end_func.name());
  result_id_ = end_func.output().data_id();

  int num_fields = input_schema_list_.front()->num_fields();
  std::map<int32_t, arrow::Datum::Kind> data_id_map = {
      {0, arrow::Datum::Kind::RECORD_BATCH}};

  func_list_.reserve(compute_trace_.func_traces_size());
  for (int i = 0; i < compute_trace_.func_traces_size(); ++i) {
    const auto& func = compute_trace_.func_traces(i);
    compute::ExtendFunctionName ex_func_name;
    auto input_kinds = BuildInputDatumKind(func.inputs(), data_id_map);
    SERVING_ENFORCE(!input_kinds.empty(), errors::ErrorCode::LOGIC_ERROR);
    if (compute::ExtendFunctionName_Parse(func.name(), &ex_func_name)) {
      // check ext func inputs type valid
      SERVING_ENFORCE(input_kinds[0] == arrow::Datum::Kind::RECORD_BATCH,
                      errors::ErrorCode::LOGIC_ERROR);
      if (ex_func_name == compute::ExtendFunctionName::EFN_TB_COLUMN ||
          ex_func_name == compute::ExtendFunctionName::EFN_TB_REMOVE_COLUMN) {
        // std::shared_ptr<Array> column(int) const
        //
        // Result<std::shared_ptr<RecordBatch>> RemoveColumn(int) const
        //
        SERVING_ENFORCE(func.inputs_size() == 2,
                        errors::ErrorCode::INVALID_ARGUMENT);
        // check index valid
        SERVING_ENFORCE(input_kinds[1] == arrow::Datum::Kind::SCALAR,
                        errors::ErrorCode::LOGIC_ERROR);
        const auto& index_scalar = func.inputs(1).custom_scalar();
        SERVING_ENFORCE(index_scalar.has_i64(), errors::ErrorCode::LOGIC_ERROR);
        // 0 <= index < num_fields
        SERVING_ENFORCE_GE(index_scalar.i64(), 0);
        SERVING_ENFORCE_LT(index_scalar.i64(), num_fields);
        if (ex_func_name == compute::ExtendFunctionName::EFN_TB_REMOVE_COLUMN) {
          // change total number of fields
          --num_fields;
        }
      } else if (ex_func_name ==
                     compute::ExtendFunctionName::EFN_TB_ADD_COLUMN ||
                 ex_func_name ==
                     compute::ExtendFunctionName::EFN_TB_SET_COLUMN) {
        // Result<std::shared_ptr<RecordBatch>> AddColumn(int, std::string
        // field_name, const std::shared_ptr<Array>&) const
        //
        // Result<std::shared_ptr<RecordBatch>> SetColumn(int, const
        // std::shared_ptr<Field>&, const std::shared_ptr<Array>&) const
        //
        SERVING_ENFORCE(func.inputs_size() == 4,
                        errors::ErrorCode::INVALID_ARGUMENT);
        SERVING_ENFORCE(input_kinds[1] == arrow::Datum::Kind::SCALAR &&
                            input_kinds[2] == arrow::Datum::Kind::SCALAR,
                        errors::ErrorCode::LOGIC_ERROR);

        SERVING_ENFORCE(input_kinds[1] == arrow::Datum::Kind::SCALAR &&
                            input_kinds[2] == arrow::Datum::Kind::SCALAR,
                        errors::ErrorCode::LOGIC_ERROR);
        // check index valid
        const auto& index_scalar = func.inputs(1).custom_scalar();
        SERVING_ENFORCE(index_scalar.has_i64(), errors::ErrorCode::LOGIC_ERROR);
        // 0 <= index
        SERVING_ENFORCE_GE(index_scalar.i64(), 0);
        if (ex_func_name == compute::ExtendFunctionName::EFN_TB_ADD_COLUMN) {
          // index <= num_fields
          SERVING_ENFORCE_LE(index_scalar.i64(), num_fields);
          // change total number of fields
          ++num_fields;
        }
        if (ex_func_name == compute::ExtendFunctionName::EFN_TB_SET_COLUMN) {
          // index < num_fields
          SERVING_ENFORCE_LT(index_scalar.i64(), num_fields);
        }

        // check field name valid
        SERVING_ENFORCE(func.inputs(2).custom_scalar().value_case() ==
                            compute::Scalar::ValueCase::kS,
                        errors::ErrorCode::INVALID_ARGUMENT,
                        "{}th input must be str for func:{}", 2, func.name());
        // check data valid
        SERVING_ENFORCE(input_kinds[3] == arrow::Datum::Kind::ARRAY,
                        errors::ErrorCode::LOGIC_ERROR);
      } else {
        SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                      "invalid ext func name: {}", func.name());
      }

      auto output_kind =
          ex_func_name == compute::ExtendFunctionName::EFN_TB_COLUMN
              ? arrow::Datum::Kind::ARRAY
              : arrow::Datum::Kind::RECORD_BATCH;
      SERVING_ENFORCE(
          data_id_map.emplace(func.output().data_id(), output_kind).second,
          errors::ErrorCode::LOGIC_ERROR, "found duplicate data_id: {}",
          func.output().data_id());

      switch (ex_func_name) {
        case compute::ExtendFunctionName::EFN_TB_COLUMN: {
          func_list_.emplace_back([](arrow::Datum& result_datum,
                                     std::vector<arrow::Datum>& func_inputs) {
            result_datum = func_inputs[0].record_batch()->column(
                std::static_pointer_cast<arrow::Int64Scalar>(
                    func_inputs[1].scalar())
                    ->value);
          });
          break;
        }
        case compute::ExtendFunctionName::EFN_TB_ADD_COLUMN: {
          func_list_.emplace_back([](arrow::Datum& result_datum,
                                     std::vector<arrow::Datum>& func_inputs) {
            int64_t index = std::static_pointer_cast<arrow::Int64Scalar>(
                                func_inputs[1].scalar())
                                ->value;
            std::string field_name(
                std::static_pointer_cast<arrow::StringScalar>(
                    func_inputs[2].scalar())
                    ->view());
            std::shared_ptr<arrow::RecordBatch> new_batch;
            SERVING_GET_ARROW_RESULT(
                func_inputs[0].record_batch()->AddColumn(
                    index, std::move(field_name), func_inputs[3].make_array()),
                new_batch);
            result_datum = new_batch;
          });
          break;
        }
        case compute::ExtendFunctionName::EFN_TB_REMOVE_COLUMN: {
          func_list_.emplace_back([](arrow::Datum& result_datum,
                                     std::vector<arrow::Datum>& func_inputs) {
            std::shared_ptr<arrow::RecordBatch> new_batch;
            SERVING_GET_ARROW_RESULT(
                func_inputs[0].record_batch()->RemoveColumn(
                    std::static_pointer_cast<arrow::Int64Scalar>(
                        func_inputs[1].scalar())
                        ->value),
                new_batch);
            result_datum = new_batch;
          });
          break;
        }
        case compute::ExtendFunctionName::EFN_TB_SET_COLUMN: {
          func_list_.emplace_back([](arrow::Datum& result_datum,
                                     std::vector<arrow::Datum>& func_inputs) {
            int64_t index = std::static_pointer_cast<arrow::Int64Scalar>(
                                func_inputs[1].scalar())
                                ->value;
            std::string field_name(
                std::static_pointer_cast<arrow::StringScalar>(
                    func_inputs[2].scalar())
                    ->view());
            std::shared_ptr<arrow::Array> array = func_inputs[3].make_array();
            std::shared_ptr<arrow::RecordBatch> new_batch;
            SERVING_GET_ARROW_RESULT(
                func_inputs[0].record_batch()->SetColumn(
                    index, arrow::field(std::move(field_name), array->type()),
                    array),
                new_batch);
            result_datum = new_batch;
          });
          break;
        }
        default:
          SERVING_THROW(errors::ErrorCode::UNEXPECTED_ERROR,
                        "invalid ext func name enum: {}",
                        static_cast<int>(ex_func_name));
      }
    } else {
      // check arrow compute func valid
      std::shared_ptr<arrow::compute::Function> arrow_func;
      SERVING_GET_ARROW_RESULT(
          arrow::compute::GetFunctionRegistry()->GetFunction(func.name()),
          arrow_func);

      // Noticed, we only allowed scalar type arrow compute function
      SERVING_ENFORCE(
          arrow_func->kind() == arrow::compute::Function::Kind::SCALAR,
          errors::ErrorCode::LOGIC_ERROR, "unsupported arrow compute func:{}",
          func.name());

      // check number of func arguments correct
      if (!arrow_func->arity().is_varargs) {
        SERVING_ENFORCE_EQ(func.inputs_size(), arrow_func->arity().num_args,
                           "The number of input does not match the "
                           "number required by the function({})",
                           func.name());
      } else {
        SERVING_ENFORCE_GE(func.inputs_size(), arrow_func->arity().num_args,
                           "The number of input does not meet the "
                           "minimum number required by the function({})",
                           func.name());
      }

      // check func options valid
      if (arrow_func->doc().options_required) {
        SERVING_ENFORCE(!func.option_bytes().empty(),
                        errors::ErrorCode::LOGIC_ERROR,
                        "arrow compute func:{} cannot be called without "
                        "options(empty `option_bytes`).",
                        func.name());
      }
      if (func.option_bytes().empty()) {
        func_list_.emplace_back(
            [func_name = func.name()](
                arrow::Datum& result_datum,
                std::vector<arrow::Datum>&
                    func_inputs) {  // call arrow compute func
              std::for_each(func_inputs.begin(), func_inputs.end(),
                            [](const arrow::Datum& d) {
                              SERVING_ENFORCE(d.is_value(),
                                              errors::ErrorCode::LOGIC_ERROR);
                            });
              SERVING_GET_ARROW_RESULT(
                  arrow::compute::CallFunction(func_name, func_inputs),
                  result_datum);

            });
      } else {
        arrow::Buffer option_buf(func.option_bytes());
        std::unique_ptr<arrow::compute::FunctionOptions> func_opts;
        SERVING_GET_ARROW_RESULT(
            arrow::compute::FunctionOptions::Deserialize(
                arrow_func->doc().options_class, option_buf),
            func_opts);

        func_list_.emplace_back(
            [func_name = func.name(), opt_ptr = func_opts.get(), arrow_func](
                arrow::Datum& result_datum,
                std::vector<arrow::Datum>&
                    func_inputs) {  // call arrow compute func
              std::for_each(func_inputs.begin(), func_inputs.end(),
                            [](const arrow::Datum& d) {
                              SERVING_ENFORCE(d.is_value(),
                                              errors::ErrorCode::LOGIC_ERROR);
                            });

              SERVING_GET_ARROW_RESULT(
                  arrow_func->Execute(func_inputs, opt_ptr,
                                      arrow::compute::default_exec_context()),
                  result_datum);
            });
        func_opt_map_.emplace(i, std::move(func_opts));
      }

      // check inputs invalid
      for (size_t j = 0; j < input_kinds.size(); ++j) {
        SERVING_ENFORCE(input_kinds[j] == arrow::Datum::Kind::ARRAY ||
                            input_kinds[j] == arrow::Datum::Kind::SCALAR,
                        errors::ErrorCode::LOGIC_ERROR,
                        "invalid input type for func({}) {}th arg", func.name(),
                        j);
      }

      SERVING_ENFORCE(
          data_id_map
              .emplace(func.output().data_id(), arrow::Datum::Kind::ARRAY)
              .second,
          errors::ErrorCode::LOGIC_ERROR, "found duplicate data_id: {}",
          func.output().data_id());
    }
  }
}

void ArrowProcessing::DoCompute(ComputeContext* ctx) {
  // sanity check
  SERVING_ENFORCE(ctx->inputs.size() == 1, errors::ErrorCode::LOGIC_ERROR);
  SERVING_ENFORCE(ctx->inputs.front().size() == 1,
                  errors::ErrorCode::LOGIC_ERROR);

  if (dummy_flag_) {
    ctx->output = ctx->inputs.front().front();
    return;
  }

  SPDLOG_INFO("replay compute: {}", compute_trace_.name());

  ctx->output = ReplayCompute(ctx->inputs.front().front());
}

std::shared_ptr<arrow::RecordBatch> ArrowProcessing::ReplayCompute(
    const std::shared_ptr<arrow::RecordBatch>& input) {
  SERVING_ENFORCE_GE(result_id_, 0);
  std::map<int32_t, arrow::Datum> datas = {{0, input}};

  arrow::Datum result_datum;
  for (int i = 0; i < compute_trace_.func_traces_size(); ++i) {
    const auto& func = compute_trace_.func_traces(i);
    SPDLOG_DEBUG("replay func: {}", func.ShortDebugString());
    auto func_inputs = BuildInputDatums(func.inputs(), datas);
    func_list_[i](result_datum, func_inputs);

    SERVING_ENFORCE(
        datas.emplace(func.output().data_id(), std::move(result_datum)).second,
        errors::ErrorCode::LOGIC_ERROR);
  }

  return datas[result_id_].record_batch();
}

void ArrowProcessing::BuildInputSchema() {
  input_schema_bytes_ = GetNodeBytesAttr(opts_.node_def, "input_schema_bytes");
  SERVING_ENFORCE(!input_schema_bytes_.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "get empty `input_schema_bytes`");
  auto input_schema = DeserializeSchema(input_schema_bytes_);
  for (const auto& f : input_schema->fields()) {
    CheckArrowDataTypeValid(f->type());
  }
  input_schema_list_.emplace_back(std::move(input_schema));
}

void ArrowProcessing::BuildOutputSchema() {
  output_schema_bytes_ =
      GetNodeBytesAttr(opts_.node_def, "output_schema_bytes");
  SERVING_ENFORCE(!output_schema_bytes_.empty(),
                  errors::ErrorCode::INVALID_ARGUMENT,
                  "get empty `output_schema_bytes`");
  output_schema_ = DeserializeSchema(output_schema_bytes_);
}

REGISTER_OP_KERNEL(ARROW_PROCESSING, ArrowProcessing)
REGISTER_OP(ARROW_PROCESSING, "0.0.1", "Replay secretflow compute functions")
    .Returnable()
    .BytesAttr("input_schema_bytes",
               "Serialized data of input schema(arrow::Schema)", false, false)
    .BytesAttr("output_schema_bytes",
               "Serialized data of output schema(arrow::Schema)", false, false)
    .BytesAttr("trace_content", "Serialized data of secretflow compute trace",
               false, true, "")
    .BoolAttr("content_json_flag", "Whether `trace_content` is serialized json",
              false, true, false)
    .Input("input", "")
    .Output("output", "");

}  // namespace secretflow::serving::op
