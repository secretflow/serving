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

// Version upgrade when `GraphDef` changed.
#define SERVING_GRAPH_MAJOR_VERSION 0
#define SERVING_GRAPH_MINOR_VERSION 2
#define SERVING_GRAPH_PATCH_VERSION 0

#define SERVING_STR_HELPER(x) #x
#define SERVING_STR(x) SERVING_STR_HELPER(x)

#define SERVING_GRAPH_VERSION_STRING                            \
  SERVING_STR(SERVING_GRAPH_MAJOR_VERSION)                      \
  "." SERVING_STR(SERVING_GRAPH_MINOR_VERSION) "." SERVING_STR( \
      SERVING_GRAPH_PATCH_VERSION)
