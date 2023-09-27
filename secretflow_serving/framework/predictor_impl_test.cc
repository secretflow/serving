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

#include "secretflow_serving/framework/predictor_impl.h"

#include "brpc/channel.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
#include "secretflow_serving/util/utils.h"

#include "secretflow_serving/protos/field.pb.h"

namespace secretflow::serving {

namespace op {

class MockOpKernel0 : public OpKernel {
 public:
  explicit MockOpKernel0(OpKernelOptions opts) : OpKernel(std::move(opts)) {}

  void Compute(ComputeContext* ctx) override {}
  void BuildInputSchema() override {}
  void BuildOutputSchema() override {}
};

class MockOpKernel1 : public OpKernel {
 public:
  explicit MockOpKernel1(OpKernelOptions opts) : OpKernel(std::move(opts)) {}

  void Compute(ComputeContext* ctx) override {}
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

class MockExecutable : public Executable {
 public:
  MockExecutable()
      : Executable(),
        mock_schema_(
            arrow::schema({arrow::field("test_name", arrow::utf8())})) {}

  const std::shared_ptr<const arrow::Schema>& GetInputFeatureSchema() {
    return mock_schema_;
  }

  MOCK_METHOD1(Run, void(Task&));

 private:
  std::shared_ptr<const arrow::Schema> mock_schema_;
};

class MockExecutionCore : public ExecutionCore {
 public:
  MockExecutionCore(Options opts) : ExecutionCore(std::move(opts)) {}

  MOCK_METHOD2(Execute,
               void(const apis::ExecuteRequest*, apis::ExecuteResponse*));
};

class MockPredictor : public PredictorImpl {
 public:
  MockPredictor(const Options& options) : PredictorImpl(options) {}

  std::shared_ptr<ExecuteContext> TestBuildExecCtx(
      const apis::PredictRequest* request, apis::PredictResponse* response,
      const std::string& target_id, const std::shared_ptr<Execution>& execution,
      std::shared_ptr<std::vector<std::shared_ptr<ExecuteContext>>>&
          last_exec_ctxs) {
    return BuildExecCtx(request, response, target_id, execution,
                        last_exec_ctxs);
  }

  MOCK_METHOD4(AsyncCallRpc,
               void(const std::string&, std::shared_ptr<brpc::Controller>&,
                    const apis::ExecuteRequest*, apis::ExecuteResponse*));

  MOCK_METHOD2(JoinAsyncCall, void(const std::string&,
                                   const std::shared_ptr<brpc::Controller>&));

  MOCK_METHOD1(CancelAsyncCall, void(const std::shared_ptr<brpc::Controller>&));
};

class PredictorImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    MockExecutionCore::Options exec_opts{"test_id", "alice", std::nullopt,
                                         std::nullopt,
                                         std::make_shared<MockExecutable>()};
    mock_exec_core_ = new MockExecutionCore(std::move(exec_opts));
    exec_core_ = std::shared_ptr<ExecutionCore>(mock_exec_core_);

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
    std::map<std::string, std::shared_ptr<Node>> nodes;
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
        n_iter->second->SetOutEdge(edge);
        pair.second->AddInEdge(edge);
      }
    }
    std::vector<std::shared_ptr<Execution>> executions;
    for (size_t i = 0; i < execution_def_jsons.size(); ++i) {
      ExecutionDef executino_def;
      JsonToPb(execution_def_jsons[i], &executino_def);

      std::map<std::string, std::shared_ptr<Node>> e_nodes;
      for (const auto& n : executino_def.nodes()) {
        e_nodes.emplace(n, nodes.find(n)->second);
      }

      executions.emplace_back(std::make_shared<Execution>(
          i, std::move(executino_def), std::move(e_nodes)));
    }

    // mock channel
    channel_map_ = std::make_shared<PartyChannelMap>();
    auto channel = std::make_unique<brpc::Channel>();
    channel_map_->emplace(std::make_pair("bob", std::move(channel)));

    p_opts_.party_id = "alice";
    p_opts_.channels = channel_map_;
    p_opts_.executions = std::move(executions);

    mock_predictor_ = std::make_shared<MockPredictor>(p_opts_);
    mock_predictor_->SetExecutionCore(exec_core_);
  }

  void TearDown() override { exec_core_ = nullptr; }

 protected:
  Predictor::Options p_opts_;

  MockExecutionCore* mock_exec_core_;

  std::shared_ptr<MockPredictor> mock_predictor_;
  std::shared_ptr<ExecutionCore> exec_core_;
  std::shared_ptr<PartyChannelMap> channel_map_;
};

MATCHER_P(ExecuteRequestEquel, expect, "") {
  return arg->service_spec().id() == expect->service_spec().id() &&
         arg->requester_id() == expect->requester_id() &&
         arg->feature_source().type() == expect->feature_source().type() &&
         std::equal(
             arg->feature_source().fs_param().query_datas().begin(),
             arg->feature_source().fs_param().query_datas().end(),
             expect->feature_source().fs_param().query_datas().begin()) &&
         arg->feature_source().fs_param().query_context() ==
             expect->feature_source().fs_param().query_context() &&
         std::equal(arg->feature_source().predefineds().begin(),
                    arg->feature_source().predefineds().end(),
                    expect->feature_source().predefineds().begin(),
                    [](const Feature& f1, const Feature& f2) {
                      return f1.field().name() == f2.field().name();
                    }) &&
         arg->task().execution_id() == expect->task().execution_id() &&
         std::equal(
             arg->task().nodes().begin(), arg->task().nodes().end(),
             expect->task().nodes().begin(),
             [](const apis::NodeIo& n1, const apis::NodeIo& n2) {
               return n1.name() == n2.name() &&
                      std::equal(
                          n1.ios().begin(), n1.ios().end(), n2.ios().begin(),
                          [](const apis::IoData& io1, const apis::IoData& io2) {
                            return std::equal(io1.datas().begin(),
                                              io1.datas().end(),
                                              io2.datas().begin());
                          });
             });
}

TEST_F(PredictorImplTest, BuildExecCtx) {
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
  std::shared_ptr<std::vector<std::shared_ptr<MockPredictor::ExecuteContext>>>
      last_ctxs;
  auto ctx_bob = mock_predictor_->TestBuildExecCtx(
      &request, &response, "bob", p_opts_.executions[0], last_ctxs);
  ASSERT_EQ(request.header().data().at("test-k"),
            ctx_bob->exec_req->header().data().at("test-k"));
  ASSERT_EQ(ctx_bob->exec_req->service_spec().id(),
            request.service_spec().id());
  ASSERT_EQ(ctx_bob->exec_req->requester_id(), p_opts_.party_id);
  ASSERT_TRUE(ctx_bob->exec_req->feature_source().type() ==
              apis::FeatureSourceType::FS_SERVICE);
  ASSERT_TRUE(std::equal(
      ctx_bob->exec_req->feature_source().fs_param().query_datas().begin(),
      ctx_bob->exec_req->feature_source().fs_param().query_datas().end(),
      request.fs_params().at(ctx_bob->target_id).query_datas().begin()));
  ASSERT_EQ(ctx_bob->exec_req->feature_source().fs_param().query_context(),
            request.fs_params().at(ctx_bob->target_id).query_context());
  ASSERT_TRUE(ctx_bob->exec_req->feature_source().predefineds().empty());
  ASSERT_EQ(ctx_bob->exec_req->task().execution_id(), 0);
  ASSERT_TRUE(ctx_bob->exec_req->task().nodes().empty());

  // build alice ctx
  auto ctx_alice = mock_predictor_->TestBuildExecCtx(
      &request, &response, "alice", p_opts_.executions[0], last_ctxs);
  ASSERT_EQ(request.header().data().at("test-k"),
            ctx_alice->exec_req->header().data().at("test-k"));
  ASSERT_EQ(ctx_alice->exec_req->service_spec().id(),
            request.service_spec().id());
  ASSERT_EQ(ctx_alice->exec_req->requester_id(), p_opts_.party_id);
  ASSERT_TRUE(ctx_alice->exec_req->feature_source().type() ==
              apis::FeatureSourceType::FS_PREDEFINED);
  ASSERT_TRUE(
      ctx_alice->exec_req->feature_source().fs_param().query_datas().empty());
  ASSERT_TRUE(
      ctx_alice->exec_req->feature_source().fs_param().query_context().empty());
  ASSERT_EQ(ctx_alice->exec_req->feature_source().predefineds_size(),
            request.predefined_features_size());
  auto f1 = ctx_alice->exec_req->feature_source().predefineds(0);
  ASSERT_FALSE(f1.field().name().empty());
  ASSERT_EQ(f1.field().name(), feature_1->field().name());
  ASSERT_EQ(f1.field().type(), feature_1->field().type());
  ASSERT_FALSE(f1.value().ss().empty());
  ASSERT_TRUE(std::equal(f1.value().ss().begin(), f1.value().ss().end(),
                         feature_1->value().ss().begin()));
  auto f2 = ctx_alice->exec_req->feature_source().predefineds(1);
  ASSERT_FALSE(f2.field().name().empty());
  ASSERT_EQ(f2.field().name(), feature_2->field().name());
  ASSERT_EQ(f2.field().type(), feature_2->field().type());
  ASSERT_FALSE(f2.value().ds().empty());
  ASSERT_TRUE(std::equal(f2.value().ds().begin(), f2.value().ds().end(),
                         feature_2->value().ds().begin()));
  ASSERT_EQ(ctx_alice->exec_req->task().execution_id(), 0);
  ASSERT_TRUE(ctx_alice->exec_req->task().nodes().empty());

  // mock alice & bob response
  {
    auto exec_response = std::make_shared<apis::ExecuteResponse>();
    exec_response->mutable_result()->set_execution_id(0);
    auto node = exec_response->mutable_result()->add_nodes();
    node->set_name("mock_node_1");
    auto io = node->add_ios();
    io->add_datas("mock_bob_data");
    ctx_bob->exec_res = exec_response;
  }
  {
    auto exec_response = std::make_shared<apis::ExecuteResponse>();
    exec_response->mutable_result()->set_execution_id(0);
    auto node_1 = exec_response->mutable_result()->add_nodes();
    node_1->set_name("mock_node_1");
    node_1->add_ios()->add_datas("mock_alice_data");
    ctx_alice->exec_res = exec_response;
  }
  auto ctx_list = std::make_shared<
      std::vector<std::shared_ptr<PredictorImpl::ExecuteContext>>>();
  ctx_list->emplace_back(ctx_bob);
  ctx_list->emplace_back(ctx_alice);

  // build ctx
  auto ctx_final = mock_predictor_->TestBuildExecCtx(
      &request, &response, "alice", p_opts_.executions[1], ctx_list);
  EXPECT_EQ(request.header().data().at("test-k"),
            ctx_final->exec_req->header().data().at("test-k"));
  EXPECT_EQ(ctx_final->exec_req->service_spec().id(),
            request.service_spec().id());
  EXPECT_EQ(ctx_final->exec_req->requester_id(), p_opts_.party_id);
  EXPECT_TRUE(ctx_final->exec_req->feature_source().type() ==
              apis::FeatureSourceType::FS_NONE);
  EXPECT_EQ(ctx_final->exec_req->task().execution_id(), 1);
  EXPECT_EQ(ctx_final->exec_req->task().nodes_size(), 1);
  auto node1 = ctx_final->exec_req->task().nodes(0);
  EXPECT_EQ(node1.name(), "mock_node_2");
  EXPECT_EQ(node1.ios_size(), 1);
  EXPECT_EQ(node1.ios(0).datas_size(), 2);
  EXPECT_EQ(node1.ios(0).datas(0), "mock_bob_data");
  EXPECT_EQ(node1.ios(0).datas(1), "mock_alice_data");
}

TEST_F(PredictorImplTest, Predict) {
  apis::PredictRequest request;
  apis::PredictResponse response;

  // mock predict request
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

  // mock bob's req & res
  apis::ExecuteResponse bob_exec0_res;
  apis::ExecuteRequest bob_exec0_req;
  {
    // build execute reponse
    bob_exec0_res.mutable_header()->mutable_data()->insert(
        {"bob-res-k", "bob-res-v"});
    bob_exec0_res.mutable_status()->set_code(1);
    bob_exec0_res.mutable_service_spec()->set_id("test_service_id");
    bob_exec0_res.mutable_result()->set_execution_id(0);
    auto node = bob_exec0_res.mutable_result()->add_nodes();
    node->set_name("mock_node_1");
    auto io = node->add_ios();
    io->add_datas("mock_bob_data");

    // build execute request
    bob_exec0_req.mutable_header()->mutable_data()->insert(
        {"test-k", "test-v"});
    *bob_exec0_req.mutable_service_spec() = request.service_spec();
    bob_exec0_req.set_requester_id(p_opts_.party_id);
    bob_exec0_req.mutable_feature_source()->set_type(
        apis::FeatureSourceType::FS_SERVICE);
    bob_exec0_req.mutable_feature_source()->mutable_fs_param()->CopyFrom(
        request.fs_params().at("bob"));
    bob_exec0_req.mutable_task()->set_execution_id(0);
  }
  // mock alice's req & res
  apis::ExecuteResponse alice_exec0_res;
  apis::ExecuteRequest alice_exec0_req;
  {
    // build execute reponse
    alice_exec0_res.mutable_header()->mutable_data()->insert(
        {"alice-res-k", "alice-res-v"});
    alice_exec0_res.mutable_status()->set_code(1);
    alice_exec0_res.mutable_service_spec()->set_id("test_service_id");
    alice_exec0_res.mutable_result()->set_execution_id(0);
    auto node = alice_exec0_res.mutable_result()->add_nodes();
    node->set_name("mock_node_1");
    auto io = node->add_ios();
    io->add_datas("mock_alice_data");

    // build execute request
    alice_exec0_req.mutable_header()->mutable_data()->insert(
        {"test-k", "test-v"});
    *alice_exec0_req.mutable_service_spec() = request.service_spec();
    alice_exec0_req.set_requester_id(p_opts_.party_id);
    alice_exec0_req.mutable_feature_source()->set_type(
        apis::FeatureSourceType::FS_PREDEFINED);
    alice_exec0_req.mutable_feature_source()->mutable_predefineds()->CopyFrom(
        request.predefined_features());
    alice_exec0_req.mutable_task()->set_execution_id(0);
  }

  // mock alice any one req & res
  apis::ExecuteResponse alice_exec1_res;
  apis::ExecuteRequest alice_exec1_req;
  {
    // build execute reponse
    alice_exec1_res.mutable_header()->mutable_data()->insert(
        {"alice-res-k", "alice-res-v"});
    alice_exec1_res.mutable_header()->mutable_data()->insert(
        {"alice-res-k1", "alice-res-v1"});
    alice_exec1_res.mutable_status()->set_code(1);
    alice_exec1_res.mutable_service_spec()->set_id("test_service_id");
    alice_exec1_res.mutable_result()->set_execution_id(1);
    auto node = alice_exec1_res.mutable_result()->add_nodes();
    node->set_name("mock_node_2");
    auto io = node->add_ios();
    auto schema = arrow::schema({arrow::field("score_0", arrow::utf8()),
                                 arrow::field("score_1", arrow::utf8())});
    arrow::StringBuilder builder;
    std::shared_ptr<arrow::Array> array;
    SERVING_CHECK_ARROW_STATUS(builder.AppendValues({"1", "2", "3"}));
    SERVING_CHECK_ARROW_STATUS(builder.Finish(&array));
    auto record_batch = MakeRecordBatch(schema, 3, {array, array});
    io->add_datas(SerializeRecordBatch(record_batch));
  }
  {
    // build execute request
    alice_exec1_req.mutable_header()->mutable_data()->insert(
        {"test-k", "test-v"});
    *alice_exec1_req.mutable_service_spec() = request.service_spec();
    alice_exec1_req.set_requester_id(p_opts_.party_id);
    alice_exec1_req.mutable_feature_source()->set_type(
        apis::FeatureSourceType::FS_NONE);
    alice_exec1_req.mutable_task()->set_execution_id(1);
    auto node = alice_exec1_req.mutable_task()->add_nodes();
    node->set_name("mock_node_2");
    auto io = node->add_ios();
    io->add_datas("mock_bob_data");
    io->add_datas("mock_alice_data");
  }

  EXPECT_CALL(*mock_predictor_,
              AsyncCallRpc(::testing::_, ::testing::_,
                           testing::Matcher<const apis::ExecuteRequest*>(
                               ExecuteRequestEquel(&bob_exec0_req)),
                           ::testing::_))
      .Times(1)
      .WillOnce(::testing::DoAll(::testing::SetArgPointee<3>(bob_exec0_res)));

  EXPECT_CALL(*mock_exec_core_,
              Execute(testing::Matcher<const apis::ExecuteRequest*>(
                          ExecuteRequestEquel(&alice_exec0_req)),
                      ::testing::_))
      .Times(1)
      .WillOnce(::testing::SetArgPointee<1>(alice_exec0_res));

  EXPECT_CALL(*mock_predictor_, JoinAsyncCall(::testing::_, ::testing::_))
      .Times(1);

  EXPECT_CALL(*mock_exec_core_,
              Execute(testing::Matcher<const apis::ExecuteRequest*>(
                          ExecuteRequestEquel(&alice_exec1_req)),
                      ::testing::_))
      .Times(1)
      .WillOnce(::testing::SetArgPointee<1>(alice_exec1_res));

  ASSERT_NO_THROW(mock_predictor_->Predict(&request, &response));
  ASSERT_EQ(response.header().data_size(), 3);
  ASSERT_EQ(response.header().data().at("bob-res-k"), "bob-res-v");
  ASSERT_EQ(response.header().data().at("alice-res-k"), "alice-res-v");
  ASSERT_EQ(response.header().data().at("alice-res-k1"), "alice-res-v1");
  ASSERT_EQ(response.header().data_size(), 3);
  ASSERT_EQ(response.results_size(), params_num);
  for (int i = 0; i < params_num; ++i) {
    auto result = response.results(i);
    ASSERT_EQ(result.scores_size(), 2);
    for (int j = 0; j < result.scores_size(); ++j) {
      ASSERT_EQ(result.scores(j).name(), "score_" + std::to_string(j));
      ASSERT_EQ(result.scores(j).value(), i + 1);
    }
  }
}

}  // namespace secretflow::serving
