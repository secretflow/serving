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

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/framework/executable.h"
#include "secretflow_serving/framework/predictor.h"

namespace secretflow::serving {

class Loader {
 public:
  struct Options {
    std::string party_id;
  };

 public:
  Loader(const Options& opts) : opts_(opts) {
    SERVING_ENFORCE(!opts_.party_id.empty(), errors::ErrorCode::LOGIC_ERROR);
  }
  virtual ~Loader() = default;

  virtual void Load(const std::string& file_path) = 0;

  virtual std::shared_ptr<Executable> GetExecutable() = 0;

  virtual std::shared_ptr<Predictor> GetPredictor() = 0;

 protected:
  Options opts_;
};

}  // namespace secretflow::serving
