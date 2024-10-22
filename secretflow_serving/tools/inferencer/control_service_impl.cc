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

#include "secretflow_serving/tools/inferencer/control_service_impl.h"

#include "brpc/closure_guard.h"
#include "brpc/controller.h"
#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/apis/error_code.pb.h"

namespace secretflow::serving::tools {

InferenceControlServiceImpl::InferenceControlServiceImpl(
    const std::string& requester_id, int64_t row_num)
    : requester_id_(requester_id), row_num_(row_num) {}

void InferenceControlServiceImpl::ReadyToServe() {
  std::lock_guard lock(mux_);
  ready_flag_ = true;
}

void InferenceControlServiceImpl::Push(
    ::google::protobuf::RpcController* controller,
    const ControlRequest* request, ControlResponse* response,
    ::google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);

  std::lock_guard lock(mux_);

  if (!ready_flag_) {
    response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
    response->mutable_status()->set_msg("waiting serving server ready");
    SPDLOG_WARN(response->status().msg());
    return;
  }

  if (request->party_id() != requester_id_) {
    response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
    response->mutable_status()->set_msg(
        fmt::format("recv control msg from {}, but requseter should be {}",
                    request->party_id(), requester_id_));
    SPDLOG_ERROR(response->status().msg());
    stop_flag_ = true;
    return;
  }
  response->mutable_status()->set_code(errors::ErrorCode::OK);

  switch (request->type()) {
    case CM_INIT: {
      SPDLOG_INFO("recv init msg from {}", request->party_id());
      init_flag_ = true;
      response->mutable_init_msg()->set_row_num(row_num_);
      break;
    }
    case CM_STOP: {
      SPDLOG_INFO("recv stop msg from {}", request->party_id());
      stop_flag_ = true;
      break;
    }
    case CM_KEEPALIVE: {
      heart_beat_count_++;
      break;
    }
    default: {
      response->mutable_status()->set_code(errors::ErrorCode::UNEXPECTED_ERROR);
      response->mutable_status()->set_msg(fmt::format(
          "unsupport msg type: {}", static_cast<int>(request->type())));
      SPDLOG_ERROR("deal request from {} failed, msg: {}", request->party_id(),
                   response->status().msg());
      stop_flag_ = true;
      break;
    }
  }
}

bool InferenceControlServiceImpl::stop_flag() {
  std::lock_guard lock(mux_);
  return stop_flag_;
}

bool InferenceControlServiceImpl::init_flag() {
  std::lock_guard lock(mux_);
  return init_flag_;
}

uint64_t InferenceControlServiceImpl::heart_beat_count() {
  std::lock_guard lock(mux_);
  return heart_beat_count_;
}

}  // namespace secretflow::serving::tools
