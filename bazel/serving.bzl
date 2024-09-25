# Copyright 2023 Ant Group Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
warpper bazel cc_xx to modify flags.
"""

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

WARNING_FLAGS = [
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wno-unused-parameter",
    "-Wnon-virtual-dtor",
] + select({
    "@bazel_tools//src/conditions:darwin": ["-Wunused-const-variable"],
    "//conditions:default": ["-Wunused-const-variable=1"],
})
DEBUG_FLAGS = ["-O0", "-g"]
RELEASE_FLAGS = ["-O2"]
FAST_FLAGS = ["-O1"]

def _serving_copts():
    return select({
        "@sf_serving//bazel:serving_build_as_release": RELEASE_FLAGS,
        "@sf_serving//bazel:serving_build_as_debug": DEBUG_FLAGS,
        "@sf_serving//bazel:serving_build_as_fast": FAST_FLAGS,
        "//conditions:default": FAST_FLAGS,
    }) + WARNING_FLAGS

def serving_cmake_external(**attrs):
    if "generate_args" not in attrs:
        attrs["generate_args"] = ["-GNinja"]
    return cmake(**attrs)

def serving_cc_binary(
        linkopts = [],
        copts = [],
        deps = [],
        **kargs):
    cc_binary(
        linkopts = linkopts + ["-ldl"],
        copts = copts + _serving_copts(),
        deps = deps,
        **kargs
    )

def serving_cc_library(
        linkopts = [],
        copts = [],
        deps = [],
        **kargs):
    cc_library(
        linkopts = linkopts,
        copts = _serving_copts() + copts,
        deps = deps + [
            "@com_github_gabime_spdlog//:spdlog",
        ],
        **kargs
    )

def serving_cc_test(
        linkopts = [],
        copts = [],
        deps = [],
        linkstatic = True,
        **kwargs):
    cc_test(
        # -lm for tcmalloc
        linkopts = linkopts + ["-lm"],
        copts = _serving_copts() + copts,
        deps = deps + [
            # use tcmalloc same as release bins. make them has same behavior on mem.
            "@com_google_googletest//:gtest_main",
        ],
        # static link for tcmalloc
        linkstatic = True,
        **kwargs
    )
