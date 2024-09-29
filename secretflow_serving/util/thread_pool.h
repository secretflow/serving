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
#include <algorithm>
#include <atomic>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"
#include "secretflow_serving/core/singleton.h"
#include "secretflow_serving/util/thread_safe_queue.h"

namespace secretflow::serving {

class ThreadPool : public Singleton<ThreadPool> {
 public:
  class Task {
   public:
    virtual ~Task() = default;

    virtual const char* Name() { return "Task"; }

    virtual void OnException(std::exception_ptr exception) noexcept = 0;

    virtual void Exec() = 0;
  };

  ~ThreadPool() { Stop(); }

  void Stop() {
    bool started = true;
    if (!started_.compare_exchange_strong(started, false)) {
      return;
    }

    if (tasks_num_ != 0) {
      SPDLOG_WARN("thread pool stoped with {} tasks not executed.",
                  tasks_num_.load());
    }

    for (auto& task_queue : task_queues_) {
      task_queue.StopPush();
    }

    for (auto& thread : threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  [[nodiscard]] bool IsRunning() const { return started_; }

  void SubmitTask(std::unique_ptr<Task> task) {
    if (!started_) {
      SPDLOG_WARN("submit task: {} while threadpool is not started",
                  task->Name());
    } else {
      SPDLOG_DEBUG("submit task: {}", task->Name());
    }

    task_queues_[insert_queue_index_.fetch_add(1) % task_queues_.size()].Push(
        std::move(task));
    tasks_num_++;
  }

  void Start(int32_t thread_num) {
    bool started = false;
    if (!started_.compare_exchange_strong(started, true)) {
      SPDLOG_WARN(
          "Thread pool cannot be started multiple times, already have {} "
          "threads running.",
          threads_.size());

      return;
    }

    SPDLOG_INFO("Create and start thread pool with {} threads", thread_num);
    task_queues_ =
        std::vector<ThreadSafeQueue<std::unique_ptr<Task>>>(thread_num);

    auto exec_task = [](auto& task) {
      SPDLOG_DEBUG("start execute: {}", task->Name());
      try {
        task->Exec();
      } catch (std::exception& e) {
        SPDLOG_ERROR("execute task {} with exception: {}", task->Name(),
                     e.what());
        task->OnException(std::current_exception());
      }
      SPDLOG_DEBUG("end execute: {}", task->Name());
    };

    for (int32_t i = 0; i != thread_num; ++i) {
      threads_.emplace_back(
          [this, exec_task](size_t i) {
            while (started_) {
              std::unique_ptr<Task> task;
              if (task_queues_[i].BlockPop(task) && started_) {
                exec_task(task);
                tasks_num_--;
              }
            }
          },
          i);
    }
  }

  [[nodiscard]] size_t GetTaskSize() const {
    return std::accumulate(
        task_queues_.begin(), task_queues_.end(), 0,
        [](int size, auto& queue) { return size + queue.size(); });
  }

 private:
  std::vector<std::thread> threads_;

  std::vector<ThreadSafeQueue<std::unique_ptr<Task>>> task_queues_;
  // Hint for submit tasks
  std::atomic<int32_t> insert_queue_index_{0};

  std::atomic<int32_t> tasks_num_{0};
  std::atomic<bool> started_{false};
};

}  // namespace secretflow::serving
