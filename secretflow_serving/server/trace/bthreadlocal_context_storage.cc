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

#include "secretflow_serving/server/trace/bthreadlocal_context_storage.h"

#include <memory>

#include "bthread/bthread.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {
namespace {

static void bthread_local_data_deleter(void* d) {
  delete static_cast<ContextStack*>(d);
}

}  // namespace

BThreadLocalContextStorage::BThreadLocalContextStorage() {
  auto ret = bthread_key_create(&tls_key_, bthread_local_data_deleter);

  SERVING_ENFORCE_EQ(ret, 0, "create bthread key failed");
}

BThreadLocalContextStorage::~BThreadLocalContextStorage() {
  bthread_key_delete(tls_key_);
}

// Return the current context.
opentelemetry::context::Context
BThreadLocalContextStorage::GetCurrent() noexcept {
  return GetStack()->Top();
}

bool BThreadLocalContextStorage::Detach(
    opentelemetry::context::Token& token) noexcept {
  // In most cases, the context to be detached is on the top of the stack.
  if (token == GetStack()->Top()) {
    GetStack()->Pop();
    return true;
  }

  if (!GetStack()->Contains(token)) {
    return false;
  }

  while (!(token == GetStack()->Top())) {
    GetStack()->Pop();
  }

  GetStack()->Pop();

  return true;
}

opentelemetry::nostd::unique_ptr<opentelemetry::context::Token>
BThreadLocalContextStorage::Attach(
    const opentelemetry::context::Context& context) noexcept {
  GetStack()->Push(context);
  return CreateToken(context);
}

ContextStack* BThreadLocalContextStorage::GetStack() {
  ContextStack* tls = static_cast<ContextStack*>(bthread_getspecific(tls_key_));
  if (tls == nullptr) {
    // First call to bthread_getspecific (and before any
    // bthread_setspecific) returns NULL.
    // Create thread-local data on demand.
    auto tmp_stack = std::make_unique<ContextStack>();

    // set the data so that next time bthread_getspecific
    // in the thread returns the data.
    tls = tmp_stack.get();
    auto ret = bthread_setspecific(tls_key_, tls);
    SERVING_ENFORCE_EQ(ret, 0, "create bthread key failed");
    tmp_stack.release();
  }

  return tls;
}

// Pops the top Context off the stack.
void ContextStack::Pop() noexcept {
  if (size_ == 0) {
    return;
  }
  // Store empty Context before decrementing `size`, to ensure
  // the shared_ptr object (if stored in prev context object ) are released.
  // The stack is not resized, and the unused memory would be reutilised
  // for subsequent context storage.
  base_[size_ - 1] = opentelemetry::context::Context();
  size_ -= 1;
}

bool ContextStack::Contains(
    opentelemetry::context::Token& token) const noexcept {
  for (size_t pos = size_; pos > 0; --pos) {
    if (token == base_[pos - 1]) {
      return true;
    }
  }

  return false;
}

// Returns the Context at the top of the stack.
opentelemetry::context::Context ContextStack::Top() const noexcept {
  if (size_ == 0) {
    return opentelemetry::context::Context();
  }
  return base_[size_ - 1];
}

// Pushes the passed in context pointer to the top of the stack
// and resizes if necessary.
void ContextStack::Push(
    const opentelemetry::context::Context& context) noexcept {
  size_++;
  if (size_ > capacity_) {
    Resize(size_ * 2);
  }
  base_[size_ - 1] = context;
}

// Reallocates the storage array to the pass in new capacity size.
void ContextStack::Resize(size_t new_capacity) noexcept {
  size_t old_size = size_ - 1;
  if (new_capacity == 0) {
    new_capacity = 2;
  }
  auto temp = std::make_unique<opentelemetry::context::Context[]>(new_capacity);
  if (base_ != nullptr) {
    // vs2015 does not like this construct considering it unsafe:
    // - std::copy(base_, base_ + old_size, temp);
    // Ref.
    // https://stackoverflow.com/questions/12270224/xutility2227-warning-c4996-std-copy-impl
    for (size_t i = 0; i < (std::min)(old_size, new_capacity); i++) {
      temp[i] = base_[i];
    }
    delete[] base_;
  }
  base_ = temp.release();
  capacity_ = new_capacity;
}

}  // namespace secretflow::serving
