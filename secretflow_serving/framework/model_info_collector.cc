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

#include "secretflow_serving/framework/model_info_collector.h"

#include <memory>
#include <thread>
#include <utility>

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/server/trace/trace.h"
#include "secretflow_serving/util/he_mgm.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/apis/model_service.pb.h"

namespace secretflow::serving {

ModelInfoCollector::ModelInfoCollector(Options opts) : opts_(std::move(opts)) {
  if (opts_.model_bundle->graph().party_num() > 0) {
    SERVING_ENFORCE_EQ(
        opts_.model_bundle->graph().party_num(),
        static_cast<int>(opts_.remote_channel_map->size()),
        "serving party num mishmatch with graph party num, {} vs {}",
        opts_.remote_channel_map->size(),
        opts_.model_bundle->graph().party_num());
  }

  // build model_info_
  model_info_.set_name(opts_.model_bundle->name());
  model_info_.set_desc(opts_.model_bundle->desc());

  auto* graph_view = model_info_.mutable_graph_view();
  graph_view->set_version(opts_.model_bundle->graph().version());
  graph_view->set_party_num(opts_.model_bundle->graph().party_num());
  graph_view->mutable_he_info()->set_pk_buf(
      opts_.model_bundle->graph().he_config().pk_buf());
  for (const auto& node : opts_.model_bundle->graph().node_list()) {
    NodeView view;
    view.set_name(node.name());
    view.set_op(node.op());
    view.set_op_version(node.op_version());
    *(view.mutable_parents()) = node.parents();

    graph_view->mutable_node_list()->Add(std::move(view));
  }
  *(graph_view->mutable_execution_list()) =
      opts_.model_bundle->graph().execution_list();

  SPDLOG_INFO("local model info: party: {} : {}", opts_.self_party_id,
              PbToJson(&model_info_));

  if (opts_.model_bundle->graph().has_he_config() &&
      !opts_.model_bundle->graph().he_config().sk_buf().empty()) {
    he::HeKitMgm::GetInstance()->InitLocalKit(
        opts_.model_bundle->graph().he_config().pk_buf(),
        opts_.model_bundle->graph().he_config().sk_buf(),
        opts_.model_bundle->graph().he_config().encode_scale());
  }
}

void ModelInfoCollector::DoCollect() {
  RetryRunner runner(max_retry_cnt_, retry_interval_ms_);
  for (auto& [remote_party_id, channel] : *(opts_.remote_channel_map)) {
    SERVING_ENFORCE(runner.Run(&ModelInfoCollector::TryCollect, *this,
                               remote_party_id, channel),
                    serving::errors::LOGIC_ERROR,
                    "GetModelInfo from {} failed.", remote_party_id);
  }

  for (const auto& [remote_party_id, model_info] : model_info_map_) {
    SPDLOG_INFO("model info: party: {} : {}", remote_party_id,
                PbToJson(&model_info));
  }

  model_info_processor_ = std::make_unique<ModelInfoProcessor>(
      opts_.self_party_id, model_info_, model_info_map_);
}

bool ModelInfoCollector::TryCollect(
    const std::string& remote_party_id,
    const std::unique_ptr<::google::protobuf::RpcChannel>& channel) {
  brpc::Controller cntl;
  // close brpc retry to make action controlled by us
  cntl.set_max_retry(0);

  apis::GetModelInfoResponse response;
  apis::GetModelInfoRequest request;
  request.mutable_service_spec()->set_id(opts_.service_id);

  auto span =
      CreateClientSpan(&cntl,
                       fmt::format("ModelService/GetModelInfo: {}-{}",
                                   opts_.self_party_id, remote_party_id),
                       request.mutable_header());

  apis::ModelService_Stub stub(channel.get());
  stub.GetModelInfo(&cntl, &request, &response, nullptr);
  SpanAttrOption span_option;
  span_option.cntl = &cntl;
  span_option.is_client = true;
  span_option.party_id = opts_.self_party_id;
  span_option.service_id = request.service_spec().id();

  if (cntl.Failed()) {
    span_option.code = errors::ErrorCode::NETWORK_ERROR;
    span_option.msg =
        fmt::format("TryCollect ({}) from ({}) GetModelInfo brpc failed.",
                    remote_party_id, opts_.self_party_id);

    SPDLOG_WARN(
        "call ({}) from ({}) GetModelInfo failed, msg:{}, may need retry",
        remote_party_id, opts_.self_party_id, cntl.ErrorText());
  } else if (!CheckStatusOk(response.status())) {
    span_option.code = response.status().code();
    span_option.msg = response.status().msg();
    SPDLOG_WARN(
        "call ({}) from ({}) GetModelInfo failed, msg:{}, may need retry",
        remote_party_id, opts_.self_party_id, response.status().msg());
  }

  SetSpanAttrs(span, span_option);
  if (span_option.code == errors::ErrorCode::OK) {
    model_info_map_[remote_party_id] = response.model_info();
    return true;
  }
  return false;
}

std::unordered_map<size_t, std::string> ModelInfoCollector::GetSpecificMap()
    const {
  SERVING_ENFORCE(model_info_processor_, serving::errors::LOGIC_ERROR,
                  "model_info is not collected properly");
  return model_info_processor_->GetSpecificMap();
}

}  // namespace secretflow::serving
