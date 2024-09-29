// Copyright 2024 Ant Group Co., Ltd.
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

#include "bthread/types.h"
#include "opentelemetry/context/runtime_context.h"

namespace secretflow::serving {

// A nested class to store the attached contexts in a stack.
// https://github.com/open-telemetry/opentelemetry-cpp/blob/main/api/include/opentelemetry/context/runtime_context.h#L186
class ContextStack {
  friend class BThreadLocalContextStorage;

 public:
  ContextStack() noexcept = default;
  ~ContextStack() noexcept { delete[] base_; }

  ContextStack(const ContextStack&) = delete;
  ContextStack& operator=(const ContextStack&) = delete;

 private:
  // Pops the top Context off the stack.
  void Pop() noexcept;

  bool Contains(opentelemetry::context::Token& token) const noexcept;

  // Returns the Context at the top of the stack.
  opentelemetry::context::Context Top() const noexcept;

  // Pushes the passed in context pointer to the top of the stack
  // and resizes if necessary.
  void Push(const opentelemetry::context::Context& context) noexcept;

  // Reallocates the storage array to the pass in new capacity size.
  void Resize(size_t new_capacity) noexcept;

 private:
  size_t size_{};
  size_t capacity_{};
  opentelemetry::context::Context* base_{nullptr};
};

// 基于bthread-local实现，
class BThreadLocalContextStorage
    : public opentelemetry::context::RuntimeContextStorage {
 public:
  BThreadLocalContextStorage();

  ~BThreadLocalContextStorage();

  // Return the current context.
  opentelemetry::context::Context GetCurrent() noexcept override;

  // Resets the context to the value previous to the passed in token. This will
  // also detach all child contexts of the passed in token.
  // Returns true if successful, false otherwise.
  bool Detach(opentelemetry::context::Token& token) noexcept override;

  // Sets the current 'Context' object. Returns a token
  // that can be used to reset to the previous Context.
  opentelemetry::nostd::unique_ptr<opentelemetry::context::Token> Attach(
      const opentelemetry::context::Context& context) noexcept override;

 private:
  ContextStack* GetStack();

  bthread_key_t tls_key_;
};

}  // namespace secretflow::serving
