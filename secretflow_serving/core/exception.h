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

#include "yacl/base/exception.h"

#include "secretflow_serving/apis/error_code.pb.h"

namespace secretflow::serving {

namespace internal {
const static int kMaxStackTraceDep = 16;

template <typename... Args>
inline std::string Format(Args... args) {
  return fmt::format(std::forward<Args>(args)...);
}

// Trick to make Format works with empty arguments.
template <>
inline std::string Format() {
  return {};
}

}  // namespace internal

class Exception : public ::yacl::Exception {
 public:
  Exception() = delete;
  explicit Exception(int code, const std::string& msg,
                     const std::string& detail)
      : ::yacl::Exception(msg), code_(code), detail_(detail) {}
  explicit Exception(int code, const std::string& msg)
      : Exception(code, msg, "") {}
  explicit Exception(int code, const std::string& msg,
                     const std::string& detail, void** stacks, int dep)
      : ::yacl::Exception(msg, stacks, dep), code_(code), detail_(detail) {}
  explicit Exception(int code, const std::string& msg, void** stacks, int dep)
      : Exception(code, msg, "", stacks, dep) {}

  int code() const noexcept { return code_; }

  const std::string& detail() const noexcept { return detail_; }

 private:
  int code_;
  std::string detail_;
};

#define SERVING_ERROR_MSG(...) \
  fmt::format("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))

#define SERVING_THROW(code, ...) \
  SERVING_THROW_WITH_STACK_TRACE(code, SERVING_ERROR_MSG(__VA_ARGS__))

#define SERVING_THROW_WITH_DETAIL(code, detail, ...)      \
  SERVING_THROW_WITH_DETAIL_AND_STACK_TRACE(code, detail, \
                                            SERVING_ERROR_MSG(__VA_ARGS__))

#define SERVING_THROW_WITH_DEFAULT_CODE(...) \
  SERVING_THROW(ErrorCode::kUnknown, __VA_ARGS__)

//
// add absl::InitializeSymbolizer to main function to get
// human-readable names stack trace
//
// Example:
// int main(int argc, char *argv[]) {
//   absl::InitializeSymbolizer(argv[0]);
//   ...
// }
//
#define SERVING_THROW_WITH_STACK_TRACE(code, msg)                       \
  do {                                                                  \
    void* stacks[::secretflow::serving::internal::kMaxStackTraceDep];   \
    int dep = absl::GetStackTrace(                                      \
        stacks, ::secretflow::serving::internal::kMaxStackTraceDep, 0); \
    throw ::secretflow::serving::Exception(code, msg, stacks, dep);     \
  } while (false)

#define SERVING_THROW_WITH_DETAIL_AND_STACK_TRACE(code, detail, msg)        \
  do {                                                                      \
    void* stacks[::secretflow::serving::internal::kMaxStackTraceDep];       \
    int dep = absl::GetStackTrace(                                          \
        stacks, ::secretflow::serving::internal::kMaxStackTraceDep, 0);     \
    throw ::secretflow::serving::Exception(code, msg, detail, stacks, dep); \
  } while (false)

//------------------------------------------------------
// ENFORCE
// https://github.com/facebookincubator/gloo/blob/master/gloo/common/logging.h

/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#define SERVING_ENFORCE_MSG(condition_str, ...)                     \
  fmt::format("[Enforce fail at {}:{}] {}. {}", __FILE__, __LINE__, \
              condition_str,                                        \
              ::secretflow::serving::internal::Format(__VA_ARGS__))

#define SERVING_ENFORCE(condition, code, ...)                                 \
  do {                                                                        \
    if (!(condition)) {                                                       \
      void* stacks[::secretflow::serving::internal::kMaxStackTraceDep];       \
      int dep = absl::GetStackTrace(                                          \
          stacks, ::secretflow::serving::internal::kMaxStackTraceDep, 0);     \
      throw ::secretflow::serving::Exception(                                 \
          code, SERVING_ENFORCE_MSG(#condition, __VA_ARGS__),                 \
          ::secretflow::serving::internal::Format(__VA_ARGS__), stacks, dep); \
    }                                                                         \
  } while (false)

/**
 * Rich logging messages
 *
 * SERVING_ENFORCE_THAT can be used with one of the "checker functions" that
 * capture input argument values and add it to the exception message. E.g.
 * `SERVING_ENFORCE_THAT(Equals(foo(x), bar(y)), "Optional additional message")`
 * would evaluate both foo and bar only once and if the results are not equal
 * - include them in the exception message.
 *
 * Some of the basic checker functions like Equals or Greater are already
 * defined below. Other header might define customized checkers by adding
 * functions to secretflow::serving::enforce_detail namespace. For example:
 *
 *   namespace secretflow::serving::enforce_detail {
 *   inline EnforceFailMessage IsVector(const vector<TIndex>& shape) {
 *     if (shape.size() == 1) { return EnforceOK(); }
 *     return fmt::format("Shape {} is not a vector", shape);
 *   }
 *
 * With further usages like `SERVING_ENFORCE_THAT(IsVector(Input(0).dims()))`
 *
 * Convenient wrappers for binary operations like SERVING_ENFORCE_EQ are
 * provided too. Please use them instead of CHECK_EQ and friends for failures in
 * user-provided input.
 */

namespace enforce {

struct EnforceOK {};

class EnforceFailMessage {
 public:
  explicit EnforceFailMessage(EnforceOK) : msg_() {}

  EnforceFailMessage(EnforceFailMessage&&) = default;
  EnforceFailMessage(const EnforceFailMessage&) = delete;
  EnforceFailMessage& operator=(EnforceFailMessage&&) = delete;
  EnforceFailMessage& operator=(const EnforceFailMessage&) = delete;

  explicit EnforceFailMessage(const std::string& msg) : msg_(msg) {}

  inline bool Bad() const { return !msg_.empty(); }

  std::string GetMessage(std::string_view extra) {
    if (extra.empty()) {
      return msg_;
    } else {
      return fmt::format("{}.{}", msg_, extra);
    }
  }

 private:
  std::string msg_;
};

#define SERVING_BINARY_COMP_HELPER(name, op)                           \
  template <typename T1, typename T2>                                  \
  inline enforce::EnforceFailMessage name(const T1& x, const T2& y) {  \
    if (x op y) {                                                      \
      return enforce::EnforceFailMessage(EnforceOK());                 \
    }                                                                  \
    return enforce::EnforceFailMessage(fmt::format("{} vs {}", x, y)); \
  }

SERVING_BINARY_COMP_HELPER(Equals, ==)
SERVING_BINARY_COMP_HELPER(NotEquals, !=)
SERVING_BINARY_COMP_HELPER(Greater, >)
SERVING_BINARY_COMP_HELPER(GreaterEquals, >=)
SERVING_BINARY_COMP_HELPER(Less, <)
SERVING_BINARY_COMP_HELPER(LessEquals, <=)
#undef SERVING_BINARY_COMP_HELPER

#define SERVING_ENFORCE_THAT_IMPL(condition, expr, code, ...)             \
  do {                                                                    \
    enforce::EnforceFailMessage r(condition);                             \
    if (r.Bad()) {                                                        \
      SERVING_THROW_WITH_STACK_TRACE(                                     \
          code,                                                           \
          SERVING_ENFORCE_MSG(                                            \
              expr, r.GetMessage(::secretflow::serving::internal::Format( \
                        __VA_ARGS__))));                                  \
    }                                                                     \
  } while (false)
}  // namespace enforce

#define SERVING_ENFORCE_THAT(condition, code, ...) \
  SERVING_ENFORCE_THAT_IMPL((condition), #condition, code, __VA_ARGS__)

#define SERVING_ENFORCE_EQ(x, y, ...)                                \
  SERVING_ENFORCE_THAT_IMPL(enforce::Equals((x), (y)), #x " == " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)
#define SERVING_ENFORCE_NE(x, y, ...)                                   \
  SERVING_ENFORCE_THAT_IMPL(enforce::NotEquals((x), (y)), #x " != " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)
#define SERVING_ENFORCE_LE(x, y, ...)                                    \
  SERVING_ENFORCE_THAT_IMPL(enforce::LessEquals((x), (y)), #x " <= " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)
#define SERVING_ENFORCE_LT(x, y, ...)                             \
  SERVING_ENFORCE_THAT_IMPL(enforce::Less((x), (y)), #x " < " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)
#define SERVING_ENFORCE_GE(x, y, ...)                                       \
  SERVING_ENFORCE_THAT_IMPL(enforce::GreaterEquals((x), (y)), #x " >= " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)
#define SERVING_ENFORCE_GT(x, y, ...)                                \
  SERVING_ENFORCE_THAT_IMPL(enforce::Greater((x), (y)), #x " > " #y, \
                            errors::ErrorCode::LOGIC_ERROR, __VA_ARGS__)

template <typename T, std::enable_if_t<std::is_pointer<T>::value, int> = 0>
T CheckNotNull(T t) {
  SERVING_ENFORCE(t != nullptr, errors::ErrorCode::LOGIC_ERROR);
  return t;
}

}  // namespace secretflow::serving
