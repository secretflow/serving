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

#include "secretflow_serving/core/singleton.h"
#include "secretflow_serving/ops/op_kernel.h"

namespace secretflow::serving::op {

class OpKernelFactory final : public Singleton<OpKernelFactory> {
 public:
  using CreateKernelFunc =
      std::function<std::shared_ptr<OpKernel>(OpKernelOptions)>;

  template <class T>
  void Register(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    SERVING_ENFORCE(creators_.find(name) == creators_.end(),
                    errors::ErrorCode::LOGIC_ERROR,
                    "duplicated op kernel registered for {}", name);
    creators_.emplace(name, [](OpKernelOptions opts) {
      return std::make_shared<T>(std::move(opts));
    });
  }

  std::shared_ptr<OpKernel> Create(OpKernelOptions opts) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto creator = creators_[opts.node->node_def().op()];
    SERVING_ENFORCE(creator, errors::ErrorCode::UNEXPECTED_ERROR,
                    "no op kernel registered for {}",
                    opts.node->node_def().op());
    return creator(std::move(opts));
  }

 private:
  std::map<std::string, CreateKernelFunc> creators_;
  std::mutex mutex_;
};

template <class T>
class KernelRegister final {
 public:
  KernelRegister(const std::string& op_name) {
    OpKernelFactory::GetInstance()->Register<T>(op_name);
  }
};

#define REGISTER_OP_KERNEL(op_name, class_name) \
  static KernelRegister<class_name> regist_kernel_##class_name(#op_name);

}  // namespace secretflow::serving::op
