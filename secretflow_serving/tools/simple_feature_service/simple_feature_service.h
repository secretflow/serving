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

#include "arrow/csv/api.h"
#include "arrow/util/io_util.h"
#include "fmt/format.h"
#include "google/protobuf/repeated_field.h"

#include "secretflow_serving/util/csv_extractor.h"

#include "secretflow_serving/apis/error_code.pb.h"
#include "secretflow_serving/spis/batch_feature_service.pb.h"

namespace secretflow::serving {

class SimpleBatchFeatureService : public spis::BatchFeatureService {
 public:
  explicit SimpleBatchFeatureService(std::string filename, std::string id_name);

  void BatchFetchFeature(::google::protobuf::RpcController *controller,
                         const spis::BatchFetchFeatureRequest *request,
                         spis::BatchFetchFeatureResponse *response,
                         ::google::protobuf::Closure *done) override;

 private:
  CSVExtractor extractor_;
};

}  // namespace secretflow::serving
