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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def sf_serving_deps():
    _bazel_platform()
    _bazel_rules_pkg()
    _rules_proto_grpc()

    _com_github_eigenteam_eigen3()
    _com_github_jupp0r_prometheus_cpp()
    _com_github_pybind11_bazel()
    _com_github_pybind11()
    _com_github_opentelemetry_cpp()

    # aws s3
    _com_aws_c_common()
    _com_aws_c_event_stream()
    _com_aws_checksums()
    _com_aws_sdk()

    _heu()
    _dataproxy()

    _com_github_brpc_brpc()

def _dataproxy():
    maybe(
        git_repository,
        name = "dataproxy",
        commit = "e38d723ab274d4568bf93cf74c0cb516435118c5",
        remote = "https://github.com/secretflow/dataproxy.git",
    )

def _heu():
    maybe(
        http_archive,
        name = "com_alipay_sf_heu",
        urls = [
            "https://github.com/secretflow/heu/archive/refs/tags/v0.5.1b0.tar.gz",
        ],
        strip_prefix = "heu-0.5.1b0",
        sha256 = "26e81b55b2d4f734977f8d5e1ba672c20287f2971dcb5d7f3fa46295e2882012",
    )

def _bazel_rules_pkg():
    http_archive(
        name = "rules_pkg",
        sha256 = "d250924a2ecc5176808fc4c25d5cf5e9e79e6346d79d5ab1c493e289e722d1d0",
        urls = [
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.10.1/rules_pkg-0.10.1.tar.gz",
        ],
    )

def _bazel_platform():
    http_archive(
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
        ],
        sha256 = "218efe8ee736d26a3572663b374a253c012b716d8af0c07e842e82f238a0a7ee",
    )

def _com_aws_c_common():
    maybe(
        http_archive,
        name = "com_aws_c_common",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/v0.9.27.tar.gz",
        ],
        sha256 = "0c0eecbd7aa04f85b1bdddf6342789bc8052737c6e9aa2ca35e26caed41d06ba",
        strip_prefix = "aws-c-common-0.9.27",
        build_file = "@sf_serving//bazel:aws_c_common.BUILD",
    )

def _com_aws_c_event_stream():
    maybe(
        http_archive,
        name = "com_aws_c_event_stream",
        urls = [
            "https://github.com/awslabs/aws-c-event-stream/archive/v0.1.5.tar.gz",
        ],
        sha256 = "f1b423a487b5d6dca118bfc0d0c6cc596dc476b282258a3228e73a8f730422d4",
        strip_prefix = "aws-c-event-stream-0.1.5",
        build_file = "@sf_serving//bazel:aws_c_event_stream.BUILD",
    )

def _com_aws_checksums():
    maybe(
        http_archive,
        name = "com_aws_checksums",
        urls = [
            "https://github.com/awslabs/aws-checksums/archive/v0.1.5.tar.gz",
        ],
        sha256 = "6e6bed6f75cf54006b6bafb01b3b96df19605572131a2260fddaf0e87949ced0",
        strip_prefix = "aws-checksums-0.1.5",
        build_file = "@sf_serving//bazel:aws_checksums.BUILD",
    )

def _com_aws_sdk():
    maybe(
        http_archive,
        name = "com_aws_sdk",
        sha256 = "758174f9788fed6cc1e266bcecb20bf738bd5ef1c3d646131c9ed15c2d6c5720",
        strip_prefix = "aws-sdk-cpp-1.7.336",
        build_file = "@sf_serving//bazel:aws.BUILD",
        urls = [
            "https://github.com/aws/aws-sdk-cpp/archive/1.7.336.tar.gz",
        ],
    )

def _com_github_eigenteam_eigen3():
    maybe(
        http_archive,
        name = "com_github_eigenteam_eigen",
        sha256 = "8586084f71f9bde545ee7fa6d00288b264a2b7ac3607b974e54d13e7162c1c72",
        build_file = "@sf_serving//bazel:eigen.BUILD",
        strip_prefix = "eigen-3.4.0",
        urls = [
            "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz",
        ],
    )

def _com_github_jupp0r_prometheus_cpp():
    maybe(
        http_archive,
        name = "com_github_jupp0r_prometheus_cpp",
        strip_prefix = "prometheus-cpp-1.2.4",
        sha256 = "48dbad454d314b836cc667ec4def93ec4a6e4255fc8387c20cacb3b8b6faee30",
        urls = [
            "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.2.4.tar.gz",
        ],
    )

def _com_github_opentelemetry_cpp():
    maybe(
        http_archive,
        name = "io_opentelemetry_cpp",
        strip_prefix = "opentelemetry-cpp-1.16.1",
        sha256 = "b8a78bb2a3a78133dbb08bcd04342f4b1e03cb4a19079b8416d408d905fffc37",
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.16.1.tar.gz",
        ],
    )

def _com_github_pybind11_bazel():
    maybe(
        http_archive,
        name = "pybind11_bazel",
        sha256 = "2d3316d89b581966fc11eab9aa9320276baee95c8233c7a8efc7158623a48de0",
        strip_prefix = "pybind11_bazel-ff261d2e9190955d0830040b20ea59ab9dbe66c8",
        urls = [
            "https://github.com/pybind/pybind11_bazel/archive/ff261d2e9190955d0830040b20ea59ab9dbe66c8.zip",
        ],
    )

def _com_github_pybind11():
    maybe(
        http_archive,
        name = "pybind11",
        build_file = "@pybind11_bazel//:pybind11.BUILD",
        sha256 = "51631e88960a8856f9c497027f55c9f2f9115cafb08c0005439838a05ba17bfc",
        strip_prefix = "pybind11-2.13.1",
        urls = [
            "https://github.com/pybind/pybind11/archive/refs/tags/v2.13.1.tar.gz",
        ],
    )

def _rules_proto_grpc():
    http_archive(
        name = "rules_proto_grpc",
        sha256 = "928e4205f701b7798ce32f3d2171c1918b363e9a600390a25c876f075f1efc0a",
        strip_prefix = "rules_proto_grpc-4.4.0",
        urls = [
            "https://github.com/rules-proto-grpc/rules_proto_grpc/releases/download/4.4.0/rules_proto_grpc-4.4.0.tar.gz",
        ],
    )

# serving not use brpc x-bd-xxx trace header, so drop the patch of yacl
# add for brpc compile
def _com_github_brpc_brpc():
    maybe(
        http_archive,
        name = "com_github_brpc_brpc",
        sha256 = "fe4eb10b4ca1a59e0f71086552b2d8897afd66df93b53c18ad83f6a93717cc2d",
        strip_prefix = "brpc-1.10.0",
        type = "tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@yacl//bazel:patches/brpc_m1.patch",
            "@sf_serving//bazel:patches/brpc.patch",
        ],
        urls = [
            "https://github.com/apache/brpc/archive/refs/tags/1.10.0.tar.gz",
        ],
    )
