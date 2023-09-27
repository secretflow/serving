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

#include <functional>
#include <map>
#include <memory>
#include <mutex>

#include "secretflow_serving/core/singleton.h"
#include "secretflow_serving/source/source.h"

namespace secretflow::serving {

using SourceCreator = std::function<std::unique_ptr<Source>(
    const ModelConfig&, const std::string&)>;

class SourceFactory : public Singleton<SourceFactory> {
 public:
  template <class T>
  void Register(SourceType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    YACL_ENFORCE(creators_.find(type) == creators_.end(),
                 "duplicated creator registered for {}", SourceType_Name(type));
    creators_.emplace(
        type, [](const ModelConfig& config, const std::string& service_id) {
          return std::make_unique<T>(config, service_id);
        });
  }

  std::unique_ptr<Source> Create(const ModelConfig& config,
                                 const std::string& service_id) {
    auto creator = creators_[config.source_type()];
    YACL_ENFORCE(creator, "no creator registered for source type: {}",
                 SourceType_Name(config.source_type()));
    return creator(config, service_id);
  }

 private:
  std::map<SourceType, SourceCreator> creators_;
  std::mutex mutex_;
};

template <class T>
class SourceRegistrar {
 public:
  explicit SourceRegistrar(SourceType type) {
    SourceFactory::GetInstance()->Register<T>(type);
  }
};

#define REGISTER_SOURCE(type, class_name) \
  static SourceRegistrar<class_name> source_registrar__##class_name(type);

}  // namespace secretflow::serving
