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

#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/recordable.h"

namespace secretflow::serving::trace {

class NoopRecordable : public opentelemetry::sdk::trace::Recordable {
 public:
  NoopRecordable() = default;
  ~NoopRecordable() override {}

  void SetIdentity(
      const opentelemetry::trace::SpanContext & /*span_context*/,
      opentelemetry::trace::SpanId /*parent_span_id*/) noexcept override {}

  void SetAttribute(opentelemetry::nostd::string_view /*key*/,
                    const opentelemetry::common::AttributeValue
                        & /*value*/) noexcept override {}

  void AddEvent(opentelemetry::nostd::string_view /*name*/,
                opentelemetry::common::SystemTimestamp /*timestamp*/,
                const opentelemetry::common::KeyValueIterable
                    & /*attributes*/) noexcept override {}

  void AddLink(const opentelemetry::trace::SpanContext & /*span_context*/,
               const opentelemetry::common::KeyValueIterable
                   & /*attributes*/) noexcept override {}

  void SetStatus(
      opentelemetry::trace::StatusCode /*code*/,
      opentelemetry::nostd::string_view /*description*/) noexcept override {}

  void SetName(opentelemetry::nostd::string_view /*name*/) noexcept override {}

  void SetSpanKind(
      opentelemetry::trace::SpanKind /*span_kind*/) noexcept override {}

  void SetResource(const opentelemetry::sdk::resource::Resource
                       & /*resource*/) noexcept override {}

  void SetStartTime(
      opentelemetry::common::SystemTimestamp /*start_time*/) noexcept override {
  }

  void SetDuration(std::chrono::nanoseconds /*duration*/) noexcept override {}

  void SetInstrumentationScope(
      const opentelemetry::sdk::trace::InstrumentationScope
          & /*instrumentation_scope*/) noexcept override {}
};

class NoopSpanProcessor : public opentelemetry::sdk::trace::SpanProcessor {
 public:
  NoopSpanProcessor() = default;

  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override {
    return std::make_unique<NoopRecordable>();
  }

  void OnStart(opentelemetry::sdk::trace::Recordable & /* span */,
               const opentelemetry::trace::SpanContext
                   & /* parent_context */) noexcept override {}

  void OnEnd(std::unique_ptr<opentelemetry::sdk::trace::Recordable>
                 &&span) noexcept override {}

  bool ForceFlush(std::chrono::microseconds timeout =
                      (std::chrono::microseconds::max)()) noexcept override {
    return true;
  }

  bool Shutdown(std::chrono::microseconds timeout =
                    (std::chrono::microseconds::max)()) noexcept override {
    return true;
  }

  ~NoopSpanProcessor() override { Shutdown(); }
};

}  // namespace secretflow::serving::trace
