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

#include <map>
#include <memory>
#include <mutex>

#include "secretflow_serving/core/singleton.h"
#include "secretflow_serving/feature_adapter/feature_adapter.h"

namespace secretflow::serving::feature {

class FeatureAdapterFactory : public Singleton<FeatureAdapterFactory> {
 public:
  using CreateAdapterFunc = std::function<std::unique_ptr<FeatureAdapter>(
      const FeatureSourceConfig&, const std::string&, const std::string&,
      const std::shared_ptr<const arrow::Schema>&)>;

  template <class T>
  void Register(FeatureSourceConfig::OptionsCase opts_case) {
    std::lock_guard<std::mutex> lock(mutex_);
    YACL_ENFORCE(creators_.find(opts_case) == creators_.end(),
                 "duplicated creator registered for {}",
                 static_cast<int>(opts_case));
    creators_.emplace(
        opts_case,
        [](const FeatureSourceConfig& spec, const std::string& service_id,
           const std::string& party_id,
           const std::shared_ptr<const arrow::Schema>& feature_schema) {
          return std::make_unique<T>(spec, service_id, party_id,
                                     feature_schema);
        });
  }

  std::unique_ptr<FeatureAdapter> Create(
      const FeatureSourceConfig& spec, const std::string& service_id,
      const std::string& party_id,
      const std::shared_ptr<const arrow::Schema>& feature_schema) {
    auto creator = creators_[spec.options_case()];
    YACL_ENFORCE(creator, "no creator registered for operator type: {}",
                 static_cast<int>(spec.options_case()));
    return creator(spec, service_id, party_id, feature_schema);
  }

 private:
  std::map<FeatureSourceConfig::OptionsCase, CreateAdapterFunc> creators_;
  std::mutex mutex_;
};

template <class T>
class Register {
 public:
  explicit Register(FeatureSourceConfig::OptionsCase opts_case) {
    FeatureAdapterFactory::GetInstance()->Register<T>(opts_case);
  }
};

#define REGISTER_ADAPTER(opts_case, class_name) \
  static Register<class_name> regist_##class_name(opts_case);

}  // namespace secretflow::serving::feature
