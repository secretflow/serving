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

#include <memory>
#include <string>
#include <thread>

#include "secretflow_serving/server/server.h"
#include "secretflow_serving/tools/inferencer/control_service_impl.h"

#include "secretflow_serving/config/serving_config.pb.h"
#include "secretflow_serving/tools/inferencer/config.pb.h"

namespace secretflow::serving::tools {

class InferenceExecutor {
 public:
  struct Options {
    ServingConfig serving_conf;

    InferenceConfig inference_conf;
  };

 public:
  explicit InferenceExecutor(Options opts);
  ~InferenceExecutor();

  void Run();

 private:
  void OnRun();

  void WaitForEnd();

  void KeepAlive();

  void StopKeepAlive();

  bool SendMsg(const std::string& target_id,
               ::google::protobuf::RpcChannel* channel, ControlMessageType type,
               ControlResponse* response);

 private:
  const Options opts_;

  std::unique_ptr<Server> server_;

  std::unique_ptr<InferenceControlServiceImpl> cntl_svc_;

  std::shared_ptr<
      std::map<std::string, std::unique_ptr<::google::protobuf::RpcChannel>>>
      channels_;

  std::shared_ptr<PredictionCore> prediction_core_;

  int32_t row_num_;

  std::thread keepalive_thread_;

  std::atomic<bool> stop_flag_ = false;
};

}  // namespace secretflow::serving::tools
