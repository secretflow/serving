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

#include "secretflow_serving/server/health.h"

#include "brpc/closure_guard.h"
#include "fmt/format.h"

namespace secretflow::serving::health {

void ServingHealthReporter::SetStatusCode(int status_code) {
  status_code_ = status_code;
}

void ServingHealthReporter::GenerateReport(brpc::Controller* cntl,
                                           google::protobuf::Closure* done) {
  brpc::ClosureGuard done_guard(done);
  cntl->http_response().set_status_code(status_code_);
  cntl->response_attachment().append(fmt::format(
      "http status code = {}", cntl->http_response().status_code()));
}

}  // namespace secretflow::serving::health
