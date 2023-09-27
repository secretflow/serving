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

#include "secretflow_serving/framework/loader.h"

namespace secretflow::serving {

class ModelLoader : public Loader {
 public:
  ModelLoader(const Options& opts, std::shared_ptr<PartyChannelMap> channels);
  virtual ~ModelLoader() = default;

  void Load(const std::string& file_path) override;

  std::shared_ptr<Executable> GetExecutable() override { return executable_; }

  std::shared_ptr<Predictor> GetPredictor() override { return predictor_; }

 private:
  std::shared_ptr<PartyChannelMap> channels_;

  std::shared_ptr<Executable> executable_;
  std::shared_ptr<Predictor> predictor_;
};

}  // namespace secretflow::serving
