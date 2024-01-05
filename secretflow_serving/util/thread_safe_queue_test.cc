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

#include "secretflow_serving/util/thread_safe_queue.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <thread>

namespace secretflow::serving {

using namespace std::chrono_literals;

TEST(ThreadSafeQueueTest, Basic) {
  ThreadSafeQueue<int> q;

  EXPECT_TRUE((std::is_same_v<int, typename ThreadSafeQueue<int>::ValueType>));

  EXPECT_EQ(q.size(), 0);
  q.Push(1);
  q.Push(2);
  EXPECT_EQ(q.size(), 2);
  int val = 0;
  q.TryPop(val);
  EXPECT_EQ(q.size(), 1);
  EXPECT_EQ(val, 1);
  q.TryPop(val);
  EXPECT_EQ(val, 2);
  EXPECT_EQ(q.size(), 0);
}

TEST(ThreadSafeQueueTest, TryPop) {
  ThreadSafeQueue<int> q;

  int val = 0;
  EXPECT_FALSE(q.TryPop(val));
  EXPECT_EQ(val, 0);

  q.Push(999);
  EXPECT_TRUE(q.TryPop(val));
  EXPECT_EQ(val, 999);
}

TEST(ThreadSafeQueueTest, WaitPop) {
  ThreadSafeQueue<int> q(50);
  int val = 0;
  auto start_time = std::chrono::steady_clock::now();
  EXPECT_FALSE(q.WaitPop(val));
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);
  EXPECT_GE(duration.count(), 50);
  EXPECT_LE(duration.count(), 60);
  q.Push(999);
  start_time = std::chrono::steady_clock::now();
  EXPECT_TRUE(q.WaitPop(val));
  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                   start_time);
  EXPECT_LE(duration.count(), 1);
}

TEST(ThreadSafeQueueTest, Push) {
  ThreadSafeQueue<uint32_t> q;
  uint32_t val = 0;

  auto done = std::async(std::launch::async, [&]() {
    for (uint32_t i = 0; i < kMaxQueueSize + 1; ++i) {
      q.Push(i);
    }
  });
  EXPECT_EQ(done.wait_for(std::chrono::milliseconds(1)),
            std::future_status::timeout);
  EXPECT_EQ(q.size(), kMaxQueueSize);
  q.TryPop(val);
  EXPECT_EQ(done.wait_for(std::chrono::milliseconds(1)),
            std::future_status::ready);
  EXPECT_EQ(q.size(), kMaxQueueSize);
}

TEST(ThreadSafeQueueTest, StopPush) {
  ThreadSafeQueue<uint32_t> q;

  auto done = std::async(std::launch::async, [&]() {
    for (uint32_t i = 0; i < kMaxQueueSize + 10; ++i) {
      q.Push(i);
    }
  });
  EXPECT_EQ(done.wait_for(std::chrono::milliseconds(1)),
            std::future_status::timeout);
  EXPECT_EQ(q.size(), kMaxQueueSize);
  q.StopPush();
  EXPECT_EQ(done.wait_for(std::chrono::milliseconds(1)),
            std::future_status::ready);
  EXPECT_EQ(q.size(), kMaxQueueSize);
}

}  // namespace secretflow::serving
