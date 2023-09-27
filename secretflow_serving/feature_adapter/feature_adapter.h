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

#include <set>
#include <string>
#include <vector>

#include "arrow/api.h"

#include "secretflow_serving/core/exception.h"

#include "secretflow_serving/apis/common.pb.h"
#include "secretflow_serving/config/feature_config.pb.h"
#include "secretflow_serving/protos/feature.pb.h"

namespace secretflow::serving::feature {

class FeatureAdapter {
 public:
  struct Request {
    const apis::Header* header;
    const FeatureParam* fs_param;
  };

  struct Response {
    apis::Header* header;
    std::shared_ptr<arrow::RecordBatch> features;
  };

 public:
  FeatureAdapter(const FeatureSourceConfig& spec, const std::string& service_id,
                 const std::string& party_id,
                 const std::shared_ptr<arrow::Schema>& feature_schema);
  virtual ~FeatureAdapter() = default;

  virtual void FetchFeature(const Request& request, Response* response);

 protected:
  virtual void OnFetchFeature(const Request& request, Response* response) = 0;

  void CheckFeatureValid(const std::shared_ptr<arrow::RecordBatch>& features);

 protected:
  FeatureSourceConfig spec_;

  const std::string service_id_;
  const std::string party_id_;

  const std::shared_ptr<arrow::Schema> feature_schema_;
};

}  // namespace secretflow::serving::feature
