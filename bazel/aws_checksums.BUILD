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

load("@sf_serving//bazel:serving.bzl", "serving_cmake_external")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
)

serving_cmake_external(
    name = "aws_checksums",
    cache_entries = {
        "CMAKE_INSTALL_LIBDIR": "lib",
        "CMAKE_POSITION_INDEPENDENT_CODE": "ON",
    },
    lib_source = ":all_srcs",
    linkopts = ["-ldl"],
    out_static_libs = [
        "libaws-checksums.a",
    ],
    deps = [
        "@com_github_curl//:curl",
        "@com_github_openssl_openssl//:openssl",
        "@zlib",
    ],
)
