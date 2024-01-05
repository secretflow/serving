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

#include "secretflow_serving/framework/executor.h"

#include <utility>

#include "secretflow_serving/ops/op_factory.h"
#include "secretflow_serving/ops/op_kernel_factory.h"
#include "secretflow_serving/util/thread_pool.h"
#include "secretflow_serving/util/thread_safe_queue.h"

namespace secretflow::serving {

Executor::Executor(const std::shared_ptr<Execution>& execution)
    : execution_(execution) {
  // create op_kernel
  auto nodes = execution_->nodes();
  node_items_ = std::make_shared<
      std::unordered_map<std::string, std::shared_ptr<NodeItem>>>();

  for (const auto& [node_name, node] : nodes) {
    op::OpKernelOptions ctx{node->node_def(), node->GetOpDef()};

    auto item = std::make_shared<NodeItem>();
    item->node = node;
    item->op_kernel =
        op::OpKernelFactory::GetInstance()->Create(std::move(ctx));
    node_items_->emplace(node_name, item);
  }

  // get input schema
  const auto& entry_nodes = execution_->GetEntryNodes();
  for (const auto& node : entry_nodes) {
    const auto& node_name = node->node_def().name();
    auto iter = node_items_->find(node_name);
    SERVING_ENFORCE(iter != node_items_->end(), errors::ErrorCode::LOGIC_ERROR);
    const auto& input_schema = iter->second->op_kernel->GetAllInputSchema();
    entry_node_names_.emplace_back(node_name);
    input_schema_map_.emplace(node_name, input_schema);
  }

  if (execution_->IsEntry()) {
    // build feature schema from entry execution
    auto iter = input_schema_map_.begin();
    const auto& first_input_schema_list = iter->second;
    SERVING_ENFORCE(first_input_schema_list.size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);
    const auto& target_schema = first_input_schema_list.front();
    ++iter;
    for (; iter != input_schema_map_.end(); ++iter) {
      SERVING_ENFORCE(iter->second.size() == 1, errors::ErrorCode::LOGIC_ERROR,
                      "entry nodes should have one input table({})",
                      iter->second.size());
      const auto& schema = iter->second.front();

      SERVING_ENFORCE_EQ(
          target_schema->num_fields(), schema->num_fields(),
          "entry nodes should have same shape inputs, expect: {}, found: {}",
          target_schema->num_fields(), schema->num_fields());
      CheckReferenceFields(schema, target_schema,
                           fmt::format("entry nodes should have same input "
                                       "schema, found node:{} mismatch",
                                       iter->first));
    }
    input_feature_schema_ = target_schema;
  }
}

std::shared_ptr<std::vector<NodeOutput>> Executor::Run(
    std::shared_ptr<arrow::RecordBatch>& features) {
  SERVING_ENFORCE(execution_->IsEntry(), errors::ErrorCode::LOGIC_ERROR);
  auto inputs =
      std::unordered_map<std::string, std::shared_ptr<op::OpComputeInputs>>();
  for (size_t i = 0; i < entry_node_names_.size(); ++i) {
    auto op_inputs = std::make_shared<op::OpComputeInputs>();
    std::vector<std::shared_ptr<arrow::RecordBatch>> record_list = {features};
    op_inputs->emplace_back(std::move(record_list));
    inputs.emplace(entry_node_names_[i], std::move(op_inputs));
  }
  return Run(inputs);
}

class RunContext {
 public:
  explicit RunContext(size_t expect_result_count,
                      std::deque<std::shared_ptr<NodeItem>> buffer = {})
      : ready_nodes_(std::move(buffer)),
        expect_result_count_(expect_result_count) {}

  void AddResult(std::string node_name,
                 std::shared_ptr<arrow::RecordBatch> table) {
    {
      std::lock_guard lock(results_mtx_);

      results_.emplace_back(std::move(node_name), std::move(table));
      results_cv_.notify_all();
    }
    if (IsFinish()) {
      Stop();
    }
  }

  void AddReadyNode(std::shared_ptr<NodeItem> node_item) {
    ready_nodes_.Push(std::move(node_item));
  }

  bool GetReadyNode(std::shared_ptr<NodeItem>& node_item) {
    return ready_nodes_.WaitPop(node_item);
  }

  bool IsFinish() const { return expect_result_count_ == results_.size(); }

  std::vector<NodeOutput> GetResults() {
    std::lock_guard lock(results_mtx_);
    return results_;
  }

  void Stop() { ready_nodes_.StopPush(); }

 private:
  ThreadSafeQueue<std::shared_ptr<NodeItem>> ready_nodes_;

  size_t expect_result_count_;

  mutable std::mutex results_mtx_;
  std::condition_variable results_cv_;
  std::vector<NodeOutput> results_;
};

class ExecuteScheduler : public std::enable_shared_from_this<ExecuteScheduler> {
 public:
  class ExecuteOpTask : public ThreadPool::Task {
   public:
    const char* Name() override { return "ExecuteOpTask"; }

    ExecuteOpTask(std::shared_ptr<NodeItem> node_item,
                  std::shared_ptr<ExecuteScheduler> sched)
        : node_item_(std::move(node_item)), sched_(std::move(sched)) {}

    void Exec() override { sched_->ExecuteOp(node_item_); }

    void OnException(std::exception_ptr e) noexcept override {
      sched_->SetTaskException(e);
    }

   private:
    std::shared_ptr<NodeItem> node_item_;
    std::shared_ptr<ExecuteScheduler> sched_;
  };

  ExecuteScheduler(
      std::shared_ptr<
          std::unordered_map<std::string, std::shared_ptr<NodeItem>>>
          node_items,
      uint64_t res_cnt, const std::shared_ptr<ThreadPool>& thread_pool,
      std::shared_ptr<Execution> execution)
      : node_items_(std::move(node_items)),
        context_(res_cnt),
        thread_pool_(thread_pool),
        execution_(std::move(execution)),
        propagator_(execution_->nodes()),
        sched_count_(0) {}

  void AddEntryNode(const std::shared_ptr<Node>& node,
                    std::shared_ptr<op::OpComputeInputs>& inputs) {
    auto* frame = propagator_.GetFrame(node->GetName());
    frame->compute_ctx.inputs = std::move(*inputs);
    context_.AddReadyNode(node_items_->find(node->node_def().name())->second);
  }

  void ExecuteOp(const std::shared_ptr<NodeItem>& node_item) {
    if (stop_flag_.load()) {
      return;
    }

    auto* frame = propagator_.GetFrame(node_item->node->node_def().name());

    node_item->op_kernel->Compute(&(frame->compute_ctx));
    sched_count_++;

    if (execution_->IsExitNode(node_item->node->node_def().name())) {
      context_.AddResult(node_item->node->node_def().name(),
                         frame->compute_ctx.output);
    }

    const auto& edges = node_item->node->out_edges();
    for (const auto& edge : edges) {
      CompleteOutEdge(edge, frame->compute_ctx.output);
    }
  }

  void CompleteOutEdge(const std::shared_ptr<Edge>& edge,
                       std::shared_ptr<arrow::RecordBatch> output) {
    std::shared_ptr<Node> dst_node;
    if (!execution_->TryGetNode(edge->dst_node(), &dst_node)) {
      return;
    }

    auto* child_frame = propagator_.GetFrame(dst_node->GetName());
    child_frame->compute_ctx.inputs[edge->dst_input_id()].emplace_back(
        std::move(output));

    if (child_frame->pending_count.fetch_sub(1) == 1) {
      context_.AddReadyNode(
          node_items_->find(dst_node->node_def().name())->second);
    }
  }

  void SubmitExecuteOpTask(std::shared_ptr<NodeItem>& node_item) {
    if (stop_flag_.load()) {
      return;
    }
    thread_pool_->SubmitTask(
        std::make_unique<ExecuteOpTask>(node_item, shared_from_this()));
  }

  void Schedule() {
    while (!stop_flag_.load() && !context_.IsFinish()) {
      // TODO: consider use bthread::Mutex and bthread::ConditionVariable
      //       to make this worker can switch to others
      std::shared_ptr<NodeItem> node_item;
      if (!context_.GetReadyNode(node_item)) {
        continue;
      }
      SubmitExecuteOpTask(node_item);
    }
  }

  void SetTaskException(std::exception_ptr& e) noexcept {
    bool expect_flag = false;
    // store the first exception
    if (stop_flag_.compare_exchange_strong(expect_flag, true)) {
      task_exception_ = e;
      context_.Stop();
    }
  }

  std::exception_ptr GetTaskException() { return task_exception_; }

  uint64_t GetSchedCount() { return sched_count_.load(); }
  std::vector<NodeOutput> GetResults() { return context_.GetResults(); }

 private:
  std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<NodeItem>>>
      node_items_;
  RunContext context_;
  std::shared_ptr<ThreadPool> thread_pool_;
  std::shared_ptr<Execution> execution_;
  Propagator propagator_;
  std::atomic<uint64_t> sched_count_{0};
  static constexpr uint64_t READY_NODE_WAIT_MS = 10;
  std::atomic<bool> stop_flag_{false};
  std::exception_ptr task_exception_;
};

std::shared_ptr<std::vector<NodeOutput>> Executor::Run(
    std::unordered_map<std::string, std::shared_ptr<op::OpComputeInputs>>&
        inputs) {
  std::vector<std::shared_ptr<op::OpComputeInputs>> entry_node_inputs;
  for (const auto& node : execution_->GetEntryNodes()) {
    auto iter = inputs.find(node->node_def().name());
    SERVING_ENFORCE(iter != inputs.end(), errors::ErrorCode::INVALID_ARGUMENT,
                    "can not found inputs for node:{}",
                    node->node_def().name());
    entry_node_inputs.emplace_back(iter->second);
  }

  auto sched = std::make_shared<ExecuteScheduler>(
      node_items_, execution_->GetExitNodeNum(), ThreadPool::GetInstance(),
      execution_);
  const auto& entry_nodes = execution_->GetEntryNodes();
  for (size_t i = 0; i != execution_->GetEntryNodeNum(); ++i) {
    sched->AddEntryNode(entry_nodes[i], entry_node_inputs[i]);
  }

  sched->Schedule();

  auto task_exception = sched->GetTaskException();
  if (task_exception) {
    SPDLOG_ERROR("Execution {} run with exception.", execution_->id());
    std::rethrow_exception(task_exception);
  }
  SERVING_ENFORCE_EQ(sched->GetSchedCount(), execution_->nodes().size());
  return std::make_shared<std::vector<NodeOutput>>(sched->GetResults());
}

}  // namespace secretflow::serving
