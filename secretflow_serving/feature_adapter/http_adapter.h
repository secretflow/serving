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

#include "google/protobuf/service.h"

#include "secretflow_serving/feature_adapter/feature_adapter.h"

#include "secretflow_serving/protos/feature.pb.h"

namespace secretflow::serving::feature {

class HttpFeatureAdapter : public FeatureAdapter {
 public:
  HttpFeatureAdapter(const FeatureSourceConfig& spec,
                     const std::string& service_id, const std::string& party_id,
                     const std::shared_ptr<arrow::Schema>& feature_schema);
  ~HttpFeatureAdapter() = default;

 protected:
  void OnFetchFeature(const Request& request, Response* response) override;

  std::string SerializeRequest(const Request& request);

  void DeserializeResponse(const std::string& res_context, Response* response);

 protected:
  std::shared_ptr<google::protobuf::RpcChannel> channel_;
  std::vector<FeatureField> feature_fields_;
};

}  // namespace secretflow::serving::feature