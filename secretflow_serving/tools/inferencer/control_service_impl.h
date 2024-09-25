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

#pragma once

#include <mutex>

#include "secretflow_serving/tools/inferencer/inference_service.pb.h"

namespace secretflow::serving::tools {

// 预测 - 服务入口
class InferenceControlServiceImpl : public InferenceControlService {
 public:
  InferenceControlServiceImpl(const std::string& requester_id, int32_t row_num);

  void Push(::google::protobuf::RpcController* controller,
            const ControlRequest* request, ControlResponse* response,
            ::google::protobuf::Closure* done) override;

  void ReadyToServe();

  [[nodiscard]] bool stop_flag();

  [[nodiscard]] bool init_flag();

  uint64_t heart_beat_count();

 private:
  std::mutex mux_;

  const std::string requester_id_;

  const int32_t row_num_;

  bool ready_flag_ = false;

  bool init_flag_ = false;

  bool stop_flag_ = false;

  uint64_t heart_beat_count_ = 0;
};

}  // namespace secretflow::serving::tools
