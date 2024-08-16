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

#include "secretflow_serving/framework/predictor.h"

#include "brpc/channel.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/arrow_helper.h"
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

class MockExecutable : public Executable {
 public:
  MockExecutable()
      : Executable({}),
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

class MockRemoteExecute : public RemoteExecute {
 public:
  using RemoteExecute::RemoteExecute;
  void Run() override {}
  void Cancel() override {}
  void WaitToFinish() override {
    exec_ctx_.CheckAndUpdateResponse(mock_exec_res);
  }
  void GetOutputs(
      std::unordered_map<std::string, std::shared_ptr<apis::NodeIo>>*
          node_io_map) override {
    ExeResponseToIoMap(mock_exec_res, node_io_map);
  }
  apis::ExecuteResponse mock_exec_res;
};

class MockPredictor : public Predictor {
 public:
  MockPredictor(const Options& options) : Predictor(options) {}
  std::shared_ptr<RemoteExecute> BuildRemoteExecute(
      const apis::PredictRequest* request, apis::PredictResponse* response,
      const std::shared_ptr<Execution>& execution, std::string target_id,
      std::shared_ptr<::google::protobuf::RpcChannel> channel) override {
    auto exec = std::make_shared<MockRemoteExecute>(
        request, response, execution, target_id, opts_.party_id, channel);
    exec->mock_exec_res = remote_exec_res_;
    return exec;
  }

  apis::ExecuteResponse remote_exec_res_;
};

bool ExeRequestEqual(const apis::ExecuteRequest* arg,
                     const apis::ExecuteRequest* expect) {
  if (arg->service_spec().id() != expect->service_spec().id()) {
    return false;
  }
  if (arg->requester_id() != expect->requester_id()) {
    return false;
  }
  if (arg->feature_source().type() != expect->feature_source().type()) {
    return false;
  }
  if (!std::equal(arg->feature_source().fs_param().query_datas().begin(),
                  arg->feature_source().fs_param().query_datas().end(),
                  expect->feature_source().fs_param().query_datas().begin())) {
    return false;
  }
  if (arg->feature_source().fs_param().query_context() !=
      expect->feature_source().fs_param().query_context()) {
    return false;
  }
  if (arg->feature_source().predefineds().size() !=
      expect->feature_source().predefineds().size()) {
    return false;
  }

  if (!std::equal(arg->feature_source().predefineds().begin(),
                  arg->feature_source().predefineds().end(),
                  expect->feature_source().predefineds().begin(),
                  [](const Feature& f1, const Feature& f2) {
                    return f1.field().name() == f2.field().name();
                  })) {
    return false;
  }
  if (arg->task().execution_id() != expect->task().execution_id()) {
    return false;
  }
  if (!std::equal(arg->task().nodes().begin(), arg->task().nodes().end(),
                  expect->task().nodes().begin(),
                  [](const apis::NodeIo& n1, const apis::NodeIo& n2) {
                    return n1.name() == n2.name() &&
                           std::equal(n1.ios().begin(), n1.ios().end(),
                                      n2.ios().begin(),
                                      [](const apis::IoData& io1,
                                         const apis::IoData& io2) {
                                        return std::equal(io1.datas().begin(),
                                                          io1.datas().end(),
                                                          io2.datas().begin());
                                      });
                  })) {
    return false;
  }
  return true;
}

MATCHER_P(ExecuteRequestEqual, expect, "") {
  return ExeRequestEqual(arg, expect);
}

class PredictorTest : public ::testing::Test {
 protected:
  void SetUpEnv(std::vector<std::string>& node_def_jsons,
                std::vector<std::string>& execution_def_jsons) {
    MockExecutionCore::Options exec_opts{"test_id", "alice", std::nullopt,
                                         std::nullopt,
                                         std::make_shared<MockExecutable>()};
    mock_exec_core_ = new MockExecutionCore(std::move(exec_opts));
    exec_core_ = std::shared_ptr<ExecutionCore>(mock_exec_core_);

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
  void SetUp() override {}
  void TearDown() override { exec_core_ = nullptr; }

 protected:
  Predictor::Options p_opts_;

  MockExecutionCore* mock_exec_core_;

  std::shared_ptr<MockPredictor> mock_predictor_;
  std::shared_ptr<ExecutionCore> exec_core_;
  std::shared_ptr<PartyChannelMap> channel_map_;
};

TEST_F(PredictorTest, Predict) {
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

  std::vector<std::vector<std::string>> execution_def_jsons_list = {{
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
)JSON"},
                                                                    {
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
    "dispatch_type": "DP_SPECIFIED",
    "specific_flag": true
  }
}
)JSON"}};
  for (auto& execution_def_jsons : execution_def_jsons_list) {
    SetUpEnv(node_def_jsons, execution_def_jsons);
    apis::PredictRequest request;
    apis::PredictResponse response;

    // mock predict request
    request.mutable_header()->mutable_data()->insert({"test-k", "test-v"});
    request.mutable_service_spec()->set_id("test_service_id");
    request.mutable_fs_params()->insert({"bob", {}});
    request.mutable_fs_params()->at("bob").set_query_context(
        "bob_test_context");
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
      // local first
      io->add_datas("mock_alice_data");
      io->add_datas("mock_bob_data");
    }

    EXPECT_CALL(*mock_exec_core_,
                Execute(testing::Matcher<const apis::ExecuteRequest*>(
                            ExecuteRequestEqual(&alice_exec0_req)),
                        ::testing::_))
        .Times(1)
        .WillOnce(::testing::SetArgPointee<1>(alice_exec0_res));

    EXPECT_CALL(*mock_exec_core_,
                Execute(testing::Matcher<const apis::ExecuteRequest*>(
                            ExecuteRequestEqual(&alice_exec1_req)),
                        ::testing::_))
        .Times(1)
        .WillOnce(::testing::SetArgPointee<1>(alice_exec1_res));

    mock_predictor_->remote_exec_res_ = bob_exec0_res;
    ASSERT_NO_THROW(mock_predictor_->Predict(&request, &response));
    for (const auto& [k, v] : response.header().data()) {
      std::cout << k << " : " << v << std::endl;
    }
    ASSERT_EQ(response.header().data_size(), 3);
    ASSERT_EQ(response.header().data().at("bob-res-k"), "bob-res-v");
    ASSERT_EQ(response.header().data().at("alice-res-k"), "alice-res-v");
    ASSERT_EQ(response.header().data().at("alice-res-k1"), "alice-res-v1");
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
}

}  // namespace secretflow::serving
