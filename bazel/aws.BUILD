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

# Description:
#  AWS C++ SDK
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@sf_serving//bazel:common.bzl", "template_rule")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "aws",
    srcs = glob([
        "aws-cpp-sdk-core/source/platform/linux-shared/*.cpp",
        "aws-cpp-sdk-core/source/net/linux-shared/*.cpp",
        "aws-cpp-sdk-core/include/**/*.h",
        "aws-cpp-sdk-core/source/*.cpp",
        "aws-cpp-sdk-core/source/auth/**/*.cpp",
        "aws-cpp-sdk-core/source/config/**/*.cpp",
        "aws-cpp-sdk-core/source/client/**/*.cpp",
        "aws-cpp-sdk-core/source/external/**/*.cpp",
        "aws-cpp-sdk-core/source/internal/**/*.cpp",
        "aws-cpp-sdk-core/source/http/*.cpp",
        "aws-cpp-sdk-core/source/http/curl/**/*.cpp",
        "aws-cpp-sdk-core/source/http/standard/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/*.cpp",
        "aws-cpp-sdk-core/source/utils/base64/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/event/*.cpp",
        "aws-cpp-sdk-core/source/utils/json/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/logging/*.cpp",
        "aws-cpp-sdk-core/source/utils/logging/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/memory/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/stream/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/threading/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/xml/**/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/factory/**/*.cpp",
        "aws-cpp-sdk-s3/include/**/*.h",
        "aws-cpp-sdk-s3/source/**/*.cpp",
        "aws-cpp-sdk-transfer/include/**/*.h",
        "aws-cpp-sdk-transfer/source/**/*.cpp",
        "aws-cpp-sdk-core/source/monitoring/*.cpp",
        "aws-cpp-sdk-core/source/utils/memory/*.cpp",
        "aws-cpp-sdk-core/source/utils/crypto/openssl/*.cpp",
    ]),
    hdrs = [
        "aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    ],
    copts = [
        "-DENABLE_OPENSSL_ENCRYPTION",
        "-DAWS_SDK_VERSION_MAJOR=1",
        "-DAWS_SDK_VERSION_MINOR=7",
        "-DAWS_SDK_VERSION_PATCH=266",
        "-DOPENSSL_IS_BORINGSSL",
    ],
    defines = [
        "PLATFORM_LINUX",
        "ENABLE_CURL_CLIENT",
        "OPENSSL_IS_BORINGSSL",
    ],
    includes = [
        "aws-cpp-sdk-core/include/",
        "aws-cpp-sdk-s3/include/",
        "aws-cpp-sdk-transfer/include/",
    ],
    linkopts = ["-ldl"],
    deps = [
        "@com_aws_c_event_stream//:aws_c_event_stream",
        "@com_aws_checksums//:aws_checksums",
        "@com_github_curl//:curl",
        "@com_github_openssl_openssl//:openssl",
        "@zlib",
    ],
)

template_rule(
    name = "SDKConfig_h",
    src = "aws-cpp-sdk-core/include/aws/core/SDKConfig.h.in",
    out = "aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    substitutions = {
        "cmakedefine": "define",
    },
)
