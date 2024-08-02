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

#include "secretflow_serving/framework/execute_context.h"

#include "brpc/channel.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/utils.h"

namespace secretflow::serving {

namespace op {

class MockOpKernel0 : public OpKernel {
 public:
  explicit MockOpKernel0(OpKernelOptions opts) : OpKernel(std::move(opts)) {}

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel1 : public OpKernel {
 public:
  explicit MockOpKernel1(OpKernelOptions opts) : OpKernel(std::move(opts)) {}

  void DoCompute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

REGISTER_OP_KERNEL(TEST_OP_0, MockOpKernel0);
REGISTER_OP_KERNEL(TEST_OP_1, MockOpKernel1);
REGISTER_OP(TEST_OP_0, "0.0.1", "test_desc")
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");
REGISTER_OP(TEST_OP_1, "0.0.1", "test_desc")
    .Mergeable()
    .Returnable()
    .StringAttr("attr_s", "attr_s_desc", false, false)
    .Input("input", "input_desc")
    .Output("output", "output_desc");

}  // namespace op

class MockExecuteContext : public ExecuteContext {
 public:
  using ExecuteContext::ExecuteContext;
  const apis::ExecuteRequest& ExeRequest() const { return exec_req_; }
  apis::ExecuteResponse& ExeResponse() { return exec_res_; }
  const apis::ExecuteResponse& ExeResponse() const { return exec_res_; }
};

class ExecuteContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // mock execution
    std::vector<std::string> node_def_jsons = {
        R"JSON(
{
  "name": "mock_node_1",
  "op": "TEST_OP_0",
}
)JSON",
        R"JSON(
{
  "name": "mock_node_2",
  "op": "TEST_OP_1",
  "parents": [ "mock_node_1" ],
}
)JSON"};

    std::vector<std::string> execution_def_jsons = {
        R"JSON(
{
  "nodes": [
    "mock_node_1"
  ],
  "config": {
    "dispatch_type": "DP_ALL"
  }
}
)JSON",
        R"JSON(
{
  "nodes": [
    "mock_node_2"
  ],
  "config": {
    "dispatch_type": "DP_ANYONE"
  }
}
)JSON"};

    // build node
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    for (const auto& j : node_def_jsons) {
      NodeDef node_def;
      JsonToPb(j, &node_def);
      auto node = std::make_shared<Node>(std::move(node_def));
      nodes.emplace(node->GetName(), node);
    }
    // build edge
    for (const auto& pair : nodes) {
      const auto& input_nodes = pair.second->GetInputNodeNames();
      for (size_t i = 0; i < input_nodes.size(); ++i) {
        auto n_iter = nodes.find(input_nodes[i]);
        SERVING_ENFORCE(n_iter != nodes.end(), errors::ErrorCode::LOGIC_ERROR);
        auto edge = std::make_shared<Edge>(n_iter->first, pair.first, i);
        n_iter->second->AddOutEdge(edge);
        pair.second->AddInEdge(edge);
      }
    }
    std::vector<std::shared_ptr<Execution>> executions;
    for (size_t i = 0; i < execution_def_jsons.size(); ++i) {
      ExecutionDef executino_def;
      JsonToPb(execution_def_jsons[i], &executino_def);

      std::unordered_map<std::string, std::shared_ptr<Node>> e_nodes;
      for (const auto& n : executino_def.nodes()) {
        e_nodes.emplace(n, nodes.find(n)->second);
      }

      executions.emplace_back(std::make_shared<Execution>(
          i, std::move(executino_def), std::move(e_nodes)));
    }

    local_id_ = "alice";
    remote_id_ = "bob";
    executions_ = std::move(executions);
  }

  void TearDown() override {}

 protected:
  std::string local_id_;
  std::string remote_id_;
  std::vector<std::shared_ptr<Execution>> executions_;
};

TEST_F(ExecuteContextTest, BuildExecCtx) {
  // mock predict request
  apis::PredictRequest request;
  apis::PredictResponse response;

  request.mutable_header()->mutable_data()->insert({"test-k", "test-v"});
  request.mutable_service_spec()->set_id("test_service_id");
  request.mutable_fs_params()->insert({"bob", {}});
  request.mutable_fs_params()->at("bob").set_query_context("bob_test_context");
  int params_num = 3;
  for (int i = 0; i < params_num; ++i) {
    request.mutable_fs_params()->at("bob").add_query_datas("bob_test_params");
  }
  auto feature_1 = request.add_predefined_features();
  feature_1->mutable_field()->set_name("feature_1");
  feature_1->mutable_field()->set_type(FieldType::FIELD_STRING);
  std::vector<std::string> ss = {"true", "false", "true"};
  feature_1->mutable_value()->mutable_ss()->Assign(ss.begin(), ss.end());
  auto feature_2 = request.add_predefined_features();
  feature_2->mutable_field()->set_name("feature_2");
  feature_2->mutable_field()->set_type(FieldType::FIELD_DOUBLE);
  std::vector<double> ds = {1.1, 2.2, 3.3};
  feature_2->mutable_value()->mutable_ds()->Assign(ds.begin(), ds.end());

  // build bob ctx
  auto ctx_bob = std::make_shared<MockExecuteContext>(
      &request, &response, executions_[0], remote_id_, local_id_);
  ASSERT_EQ(request.header().data().at("test-k"),
            ctx_bob->ExeRequest().header().data().at("test-k"));
  ASSERT_EQ(ctx_bob->ExeRequest().service_spec().id(),
            request.service_spec().id());
  ASSERT_EQ(ctx_bob->ExeRequest().requester_id(), local_id_);
  ASSERT_TRUE(ctx_bob->ExeRequest().feature_source().type() ==
              apis::FeatureSourceType::FS_SERVICE);
  ASSERT_TRUE(std::equal(
      ctx_bob->ExeRequest().feature_source().fs_param().query_datas().begin(),
      ctx_bob->ExeRequest().feature_source().fs_param().query_datas().end(),
      request.fs_params().at(ctx_bob->TargetId()).query_datas().begin()));
  ASSERT_EQ(ctx_bob->ExeRequest().feature_source().fs_param().query_context(),
            request.fs_params().at(ctx_bob->TargetId()).query_context());
  ASSERT_TRUE(ctx_bob->ExeRequest().feature_source().predefineds().empty());
  ASSERT_EQ(ctx_bob->ExeRequest().task().execution_id(), 0);
  ASSERT_TRUE(ctx_bob->ExeRequest().task().nodes().empty());

  // build alice ctx

  auto ctx_alice = std::make_shared<MockExecuteContext>(
      &request, &response, executions_[0], local_id_, local_id_);
  ASSERT_EQ(request.header().data().at("test-k"),
            ctx_alice->ExeRequest().header().data().at("test-k"));
  ASSERT_EQ(ctx_alice->ExeRequest().service_spec().id(),
            request.service_spec().id());
  ASSERT_EQ(ctx_alice->ExeRequest().requester_id(), local_id_);
  ASSERT_TRUE(ctx_alice->ExeRequest().feature_source().type() ==
              apis::FeatureSourceType::FS_PREDEFINED);
  ASSERT_TRUE(ctx_alice->ExeRequest()
                  .feature_source()
                  .fs_param()
                  .query_datas()
                  .empty());
  ASSERT_TRUE(ctx_alice->ExeRequest()
                  .feature_source()
                  .fs_param()
                  .query_context()
                  .empty());
  ASSERT_EQ(ctx_alice->ExeRequest().feature_source().predefineds_size(),
            request.predefined_features_size());
  auto f1 = ctx_alice->ExeRequest().feature_source().predefineds(0);
  ASSERT_FALSE(f1.field().name().empty());
  ASSERT_EQ(f1.field().name(), feature_1->field().name());
  ASSERT_EQ(f1.field().type(), feature_1->field().type());
  ASSERT_FALSE(f1.value().ss().empty());
  ASSERT_TRUE(std::equal(f1.value().ss().begin(), f1.value().ss().end(),
                         feature_1->value().ss().begin()));
  auto f2 = ctx_alice->ExeRequest().feature_source().predefineds(1);
  ASSERT_FALSE(f2.field().name().empty());
  ASSERT_EQ(f2.field().name(), feature_2->field().name());
  ASSERT_EQ(f2.field().type(), feature_2->field().type());
  ASSERT_FALSE(f2.value().ds().empty());
  ASSERT_TRUE(std::equal(f2.value().ds().begin(), f2.value().ds().end(),
                         feature_2->value().ds().begin()));
  ASSERT_EQ(ctx_alice->ExeRequest().task().execution_id(), 0);
  ASSERT_TRUE(ctx_alice->ExeRequest().task().nodes().empty());

  // mock alice & bob response
  {
    auto& exec_response = ctx_bob->ExeResponse();
    exec_response.mutable_result()->set_execution_id(0);
    auto node = exec_response.mutable_result()->add_nodes();
    node->set_name("mock_node_1");
    auto io = node->add_ios();
    io->add_datas("mock_bob_data");
  }
  {
    auto& exec_response = ctx_alice->ExeResponse();
    exec_response.mutable_result()->set_execution_id(0);
    auto node_1 = exec_response.mutable_result()->add_nodes();
    node_1->set_name("mock_node_1");
    node_1->add_ios()->add_datas("mock_alice_data");
  }

  std::unordered_map<std::string, std::shared_ptr<apis::NodeIo>> node_io_map;
  ctx_bob->GetResultNodeIo(&node_io_map);
  ctx_alice->GetResultNodeIo(&node_io_map);

  // build ctx
  auto ctx_final = std::make_shared<MockExecuteContext>(
      &request, &response, executions_[1], local_id_, local_id_);
  ctx_final->SetEntryNodesInputs(node_io_map);

  EXPECT_EQ(request.header().data().at("test-k"),
            ctx_final->ExeRequest().header().data().at("test-k"));
  EXPECT_EQ(ctx_final->ExeRequest().service_spec().id(),
            request.service_spec().id());
  EXPECT_EQ(ctx_final->ExeRequest().requester_id(), local_id_);
  EXPECT_TRUE(ctx_final->ExeRequest().feature_source().type() ==
              apis::FeatureSourceType::FS_NONE);
  EXPECT_EQ(ctx_final->ExeRequest().task().execution_id(), 1);
  EXPECT_EQ(ctx_final->ExeRequest().task().nodes_size(), 1);
  auto node1 = ctx_final->ExeRequest().task().nodes(0);
  EXPECT_EQ(node1.name(), "mock_node_2");
  EXPECT_EQ(node1.ios_size(), 1);
  EXPECT_EQ(node1.ios(0).datas_size(), 2);
  EXPECT_EQ(node1.ios(0).datas(0), "mock_bob_data");
  EXPECT_EQ(node1.ios(0).datas(1), "mock_alice_data");
}

}  // namespace secretflow::serving
