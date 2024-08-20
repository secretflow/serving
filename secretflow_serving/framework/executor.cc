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

class RunContext {
 public:
  explicit RunContext(size_t expect_result_count)
      : expect_result_count_(expect_result_count) {}

  void AddResult(std::string node_name,
                 std::shared_ptr<arrow::RecordBatch> table) {
    std::lock_guard lock(results_mtx_);
    results_.emplace_back(std::move(node_name), std::move(table));
    if (IsFinish()) {
      Stop();
    }
  }

  bool IsFinish() const { return expect_result_count_ == results_.size(); }

  void WaitFinish() {
    std::unique_lock lock(results_mtx_);
    results_cv_.wait(lock, [this] { return stop_flag_ || IsFinish(); });
  }

  std::vector<NodeOutput> GetResults() {
    std::lock_guard lock(results_mtx_);
    return results_;
  }

  void Stop() {
    stop_flag_ = true;
    results_cv_.notify_all();
  }

 private:
  size_t expect_result_count_;

  mutable std::mutex results_mtx_;
  std::condition_variable results_cv_;
  std::atomic<bool> stop_flag_{false};

  std::vector<NodeOutput> results_;
};

class ExecuteScheduler : public std::enable_shared_from_this<ExecuteScheduler> {
 public:
  class ExecuteOpTask : public ThreadPool::Task {
   public:
    ExecuteOpTask(std::string node_name,
                  std::shared_ptr<ExecuteScheduler> sched)
        : node_name_(std::move(node_name)), sched_(std::move(sched)) {}

    const char* Name() override { return "ExecuteOpTask"; }

    void Exec() override { sched_->ExecuteNode(node_name_); }

    void OnException(std::exception_ptr e) noexcept override {
      sched_->SetTaskException(e);
    }

   private:
    const std::string node_name_;
    std::shared_ptr<ExecuteScheduler> sched_;
  };

  ExecuteScheduler(
      const std::shared_ptr<std::unordered_map<std::string, NodeItem>>&
          node_items,
      std::shared_ptr<Execution> execution, ThreadPool* thread_pool)
      : node_items_(node_items),
        execution_(std::move(execution)),
        context_(execution_->GetOutputNodeNum()),
        thread_pool_(thread_pool),
        propagator_(execution_->nodes()),
        sched_count_(0) {}

  void ExecuteNode(const std::string& node_name) {
    if (stop_flag_.load()) {
      return;
    }

    auto* frame = propagator_.GetFrame(node_name);
    auto iter = node_items_->find(node_name);
    SERVING_ENFORCE(iter != node_items_->end(),
                    errors::ErrorCode::UNEXPECTED_ERROR);
    const auto& node_item = iter->second;
    node_item.op_kernel->Compute(&(frame->compute_ctx));
    sched_count_++;

    if (execution_->IsOutputNode(node_item.node->node_def().name())) {
      context_.AddResult(node_item.node->node_def().name(),
                         frame->compute_ctx.output);
    }

    CompleteOutEdge(node_item.node->out_edges(), frame->compute_ctx.output);
  }

  void Schedule(std::shared_ptr<arrow::RecordBatch>& features) {
    for (const auto& node : execution_->GetEntryNodes()) {
      auto* frame = propagator_.GetFrame(node->GetName());
      SERVING_ENFORCE_EQ(frame->compute_ctx.inputs.size(), 0U);
      frame->compute_ctx.inputs.emplace_back(
          std::vector<std::shared_ptr<arrow::RecordBatch>>{features});
      frame->pending_count = 0;

      SubmitTask(node->node_def().name());
    }

    context_.WaitFinish();
  }

  void Schedule(std::unordered_map<
                std::string, std::vector<std::shared_ptr<arrow::RecordBatch>>>&
                    prev_exec_outputs) {
    for (const auto& node : execution_->GetEntryNodes()) {
      auto* frame = propagator_.GetFrame(node->GetName());
      for (const auto& e : node->in_edges()) {
        auto n_iter = prev_exec_outputs.find(e->src_node());
        if (n_iter != prev_exec_outputs.end()) {
          // Do not `std::move` the vector,
          // because other nodes may also use it as an input.
          frame->compute_ctx.inputs[e->dst_input_id()] = n_iter->second;
          frame->pending_count--;
          continue;
        }
        std::shared_ptr<Node> tmp_node;
        SERVING_ENFORCE(execution_->TryGetNode(e->src_node(), &tmp_node),
                        errors::ErrorCode::LOGIC_ERROR,
                        "can not found {}'s output data", e->src_node());
      }

      if (frame->pending_count == 0) {
        SubmitTask(node->node_def().name());
      }
    }

    context_.WaitFinish();
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
  void CompleteOutEdge(const std::vector<std::shared_ptr<Edge>>& edges,
                       std::shared_ptr<arrow::RecordBatch>& output) {
    for (const auto& edge : edges) {
      std::shared_ptr<Node> dst_node;
      if (!execution_->TryGetNode(edge->dst_node(), &dst_node)) {
        continue;
      }

      auto* child_frame = propagator_.GetFrame(dst_node->GetName());
      child_frame->compute_ctx.inputs[edge->dst_input_id()].emplace_back(
          output);

      if (child_frame->pending_count.fetch_sub(1) == 1) {
        if (edges.size() > 1) {
          SubmitTask(dst_node->GetName());
        } else {
          ExecuteNode(dst_node->GetName());
        }
      }
    }
  }

  void SubmitTask(const std::string& node_name) {
    if (stop_flag_.load()) {
      return;
    }
    thread_pool_->SubmitTask(
        std::make_unique<ExecuteOpTask>(node_name, shared_from_this()));
  }

 private:
  const std::shared_ptr<std::unordered_map<std::string, NodeItem>>& node_items_;
  std::shared_ptr<Execution> execution_;

  RunContext context_;
  ThreadPool* thread_pool_;

  Propagator propagator_;
  std::atomic<uint64_t> sched_count_{0};
  std::atomic<bool> stop_flag_{false};
  std::exception_ptr task_exception_;
};

Executor::Executor(const std::shared_ptr<Execution>& execution)
    : execution_(execution) {
  // create op_kernel
  node_items_ = std::make_shared<std::unordered_map<std::string, NodeItem>>();

  for (const auto& [node_name, node] : execution_->nodes()) {
    op::OpKernelOptions ctx{node->node_def(), node->GetOpDef()};
    NodeItem item{node,
                  op::OpKernelFactory::GetInstance()->Create(std::move(ctx))};
    node_items_->emplace(node_name, std::move(item));
  }

  // get input schema
  const auto& entry_nodes = execution_->GetEntryNodes();
  for (const auto& node : entry_nodes) {
    const auto& node_name = node->node_def().name();
    auto iter = node_items_->find(node_name);
    SERVING_ENFORCE(iter != node_items_->end(), errors::ErrorCode::LOGIC_ERROR);
    const auto& input_schema = iter->second.op_kernel->GetAllInputSchema();
    input_schema_map_.emplace(node_name, input_schema);
  }

  if (execution_->IsGraphEntry()) {
    // build feature schema from entry execution
    auto iter = input_schema_map_.begin();
    const auto& first_input_schema_list = iter->second;
    SERVING_ENFORCE(first_input_schema_list.size() == 1,
                    errors::ErrorCode::LOGIC_ERROR);
    const auto& target_schema = first_input_schema_list.front();
    ++iter;
    for (; iter != input_schema_map_.end(); ++iter) {
      SERVING_ENFORCE_EQ(iter->second.size(), 1U,
                         "entry nodes should have only one input table");
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

std::vector<NodeOutput> Executor::Run(
    std::shared_ptr<arrow::RecordBatch>& features) {
  auto sched = std::make_shared<ExecuteScheduler>(node_items_, execution_,
                                                  ThreadPool::GetInstance());
  sched->Schedule(features);

  auto task_exception = sched->GetTaskException();
  if (task_exception) {
    SPDLOG_ERROR("Execution {} run with exception.", execution_->id());
    std::rethrow_exception(task_exception);
  }
  SERVING_ENFORCE_EQ(sched->GetSchedCount(), execution_->nodes().size());
  return sched->GetResults();
}

std::vector<NodeOutput> Executor::Run(
    std::unordered_map<std::string,
                       std::vector<std::shared_ptr<arrow::RecordBatch>>>&
        prev_node_outputs) {
  SERVING_ENFORCE(!execution_->IsGraphEntry(), errors::ErrorCode::LOGIC_ERROR);
  auto sched = std::make_shared<ExecuteScheduler>(node_items_, execution_,
                                                  ThreadPool::GetInstance());
  sched->Schedule(prev_node_outputs);

  auto task_exception = sched->GetTaskException();
  if (task_exception) {
    SPDLOG_ERROR("Execution {} run with exception.", execution_->id());
    std::rethrow_exception(task_exception);
  }
  SERVING_ENFORCE_EQ(sched->GetSchedCount(), execution_->nodes().size());
  return sched->GetResults();
}

}  // namespace secretflow::serving
