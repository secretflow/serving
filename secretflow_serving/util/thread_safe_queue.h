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
#include <condition_variable>
#include <deque>
#include <mutex>

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

constexpr uint32_t kMaxQueueSize = 4096;

template <typename T>
class ThreadSafeQueue {
 public:
  using ValueType = T;
  using SizeType = size_t;

  explicit ThreadSafeQueue(uint32_t pop_wait_ms = DEFAULT_POP_WAIT_MS)
      : ThreadSafeQueue({}, pop_wait_ms) {}

  explicit ThreadSafeQueue(std::deque<T> buffer,
                           uint32_t pop_wait_ms = DEFAULT_POP_WAIT_MS)
      : wait_ms_(pop_wait_ms) {
    SERVING_ENFORCE_LE(buffer.size(), kMaxQueueSize,
                       "init buffer size is too big: {} > {}", buffer.size(),
                       kMaxQueueSize);
    for (size_t i = 0; i != buffer.size(); ++i) {
      buffer_[i] = std::move(buffer[i]);
    }
    SERVING_ENFORCE(wait_ms_ > 0, errors::ErrorCode::INVALID_ARGUMENT,
                    "wait_ms should not be zero", wait_ms_);
  }

  ThreadSafeQueue(const ThreadSafeQueue&) = delete;
  ThreadSafeQueue(ThreadSafeQueue&&) = delete;
  ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
  ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

  void Push(T t) {
    // wait queue not full
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (length_ >= kMaxQueueSize) {
        full_cv_.wait(lock,
                      [this] { return length_ < kMaxQueueSize || stop_flag_; });
      }
      if (stop_flag_) {
        empty_cv_.notify_all();
        return;
      }
      length_++;
      buffer_[tail_index_] = std::move(t);
      ++tail_index_;
      tail_index_ = tail_index_ % kMaxQueueSize;
    }
    empty_cv_.notify_one();
  }

  void StopPush() {
    stop_flag_ = true;
    full_cv_.notify_all();
    empty_cv_.notify_all();
  }

  bool BlockPop(T& t) {
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (length_ <= 0) {
        empty_cv_.wait(lock, [this] { return stop_flag_ || (length_ > 0); });
        if (length_ <= 0) {
          return false;
        }
      }
      length_--;
      t = std::move(buffer_[head_index_]);

      ++head_index_;
      head_index_ = head_index_ % kMaxQueueSize;
    }
    full_cv_.notify_one();
    return true;
  }

  bool TryPop(T& t) {
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (length_ <= 0) {
        return false;
      }
      length_--;
      t = std::move(buffer_[head_index_]);
      ++head_index_;
      head_index_ = head_index_ % kMaxQueueSize;
    }
    full_cv_.notify_one();
    return true;
  }

  bool WaitPop(T& t) {
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (length_ <= 0) {
        empty_cv_.wait_for(lock, std::chrono::milliseconds(wait_ms_),
                           [this] { return stop_flag_ || (length_ > 0); });
        if (length_ <= 0) {
          return false;
        }
      }
      length_--;
      t = std::move(buffer_[head_index_]);
      ++head_index_;
      head_index_ = head_index_ % kMaxQueueSize;
    }
    full_cv_.notify_one();

    return true;
  }

  int size() const {
    std::lock_guard<std::mutex> guard_(mtx_);
    return length_;
  }

  ~ThreadSafeQueue() { StopPush(); }

 private:
  std::array<T, kMaxQueueSize> buffer_;

  mutable std::mutex mtx_;
  std::condition_variable full_cv_;
  std::condition_variable empty_cv_;
  uint32_t head_index_{0};
  uint32_t tail_index_{0};
  uint32_t length_{0};
  std::atomic<bool> stop_flag_{false};

  uint32_t wait_ms_;

  static constexpr uint32_t DEFAULT_POP_WAIT_MS = 10;
};
}  // namespace secretflow::serving
