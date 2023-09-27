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

#include "secretflow_serving/server/metrics/default_metrics_registry.h"

namespace secretflow::serving::metrics {

std::shared_ptr<::prometheus::Registry> GetDefaultRegistry() {
  static auto registry = std::make_shared<::prometheus::Registry>();
  return registry;
}

}  // namespace secretflow::serving::metrics
