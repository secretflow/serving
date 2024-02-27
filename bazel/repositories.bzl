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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")

SECRETFLOW_GIT = "https://github.com/secretflow"

YACL_COMMIT_ID = "e3401da07d8aa2f3fe2238e8563e5f573036a594"

KUSCIA_COMMIT_ID = "1979d1f4f17db5c2bd6c57be7a690e88fa9ce7ed"

def sf_serving_deps():
    _bazel_platform()
    _bazel_rules_pkg()
    _rules_proto_grpc()

    _com_github_nelhage_rules_boost()
    _com_github_facebook_zstd()
    _org_sourceware_bzip2()
    _com_github_google_brotli()
    _com_github_lz4_lz4()
    _com_github_google_snappy()
    _com_google_double_conversion()
    _com_github_tencent_rapidjson()
    _com_github_xtensor_xsimd()
    _com_github_eigenteam_eigen3()
    _com_github_jupp0r_prometheus_cpp()
    _org_apache_thrift()
    _org_apache_arrow()
    _com_github_pybind11_bazel()
    _com_github_pybind11()

    # aws s3
    _com_aws_c_common()
    _com_aws_c_event_stream()
    _com_aws_checksums()
    _com_aws_sdk()
    _com_github_curl()

    maybe(
        git_repository,
        name = "yacl",
        commit = YACL_COMMIT_ID,
        remote = "{}/yacl.git".format(SECRETFLOW_GIT),
    )

    maybe(
        git_repository,
        name = "kuscia",
        commit = KUSCIA_COMMIT_ID,
        remote = "{}/kuscia.git".format(SECRETFLOW_GIT),
    )

def _bazel_rules_pkg():
    http_archive(
        name = "rules_pkg",
        sha256 = "8f9ee2dc10c1ae514ee599a8b42ed99fa262b757058f65ad3c384289ff70c4b8",
        urls = [
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
        ],
    )

def _bazel_platform():
    http_archive(
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
        ],
        sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
    )

def _com_github_curl():
    maybe(
        http_archive,
        name = "com_github_curl",
        build_file = "@sf_serving//bazel:curl.BUILD",
        sha256 = "e9c37986337743f37fd14fe8737f246e97aec94b39d1b71e8a5973f72a9fc4f5",
        strip_prefix = "curl-7.60.0",
        urls = [
            "http://mirror.tensorflow.org/curl.haxx.se/download/curl-7.60.0.tar.gz",
            "https://curl.haxx.se/download/curl-7.60.0.tar.gz",
        ],
    )

def _com_aws_c_common():
    maybe(
        http_archive,
        name = "com_aws_c_common",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/v0.4.29.tar.gz",
        ],
        sha256 = "01c2a58553a37b3aa5914d9e0bf7bf14507ff4937bc5872a678892ca20fcae1f",
        strip_prefix = "aws-c-common-0.4.29",
        build_file = "@sf_serving//bazel:aws_c_common.BUILD",
    )

def _com_aws_c_event_stream():
    maybe(
        http_archive,
        name = "com_aws_c_event_stream",
        urls = [
            "https://github.com/awslabs/aws-c-event-stream/archive/v0.1.4.tar.gz",
        ],
        sha256 = "31d880d1c868d3f3df1e1f4b45e56ac73724a4dc3449d04d47fc0746f6f077b6",
        strip_prefix = "aws-c-event-stream-0.1.4",
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
        strip_prefix = "prometheus-cpp-1.1.0",
        urls = [
            "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.1.0.tar.gz",
        ],
    )

def _org_sourceware_bzip2():
    maybe(
        http_archive,
        name = "bzip2",
        build_file = "@sf_serving//bazel:bzip2.BUILD",
        sha256 = "ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269",
        strip_prefix = "bzip2-1.0.8",
        urls = [
            "https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz",
        ],
    )

def _com_github_lz4_lz4():
    maybe(
        http_archive,
        name = "com_github_lz4_lz4",
        sha256 = "030644df4611007ff7dc962d981f390361e6c97a34e5cbc393ddfbe019ffe2c1",
        strip_prefix = "lz4-1.9.3",
        type = "tar.gz",
        build_file = "@sf_serving//bazel:lz4.BUILD",
        urls = [
            "https://codeload.github.com/lz4/lz4/tar.gz/refs/tags/v1.9.3",
        ],
    )

def _com_google_double_conversion():
    maybe(
        http_archive,
        name = "com_google_double_conversion",
        sha256 = "a63ecb93182134ba4293fd5f22d6e08ca417caafa244afaa751cbfddf6415b13",
        strip_prefix = "double-conversion-3.1.5",
        build_file = "@sf_serving//bazel:double_conversion.BUILD",
        urls = [
            "https://github.com/google/double-conversion/archive/refs/tags/v3.1.5.tar.gz",
        ],
    )

def _com_github_xtensor_xsimd():
    maybe(
        http_archive,
        name = "com_github_xtensor_xsimd",
        sha256 = "d52551360d37709675237d2a0418e28f70995b5b7cdad7c674626bcfbbf48328",
        type = "tar.gz",
        strip_prefix = "xsimd-8.1.0",
        build_file = "@sf_serving//bazel:xsimd.BUILD",
        urls = [
            "https://codeload.github.com/xtensor-stack/xsimd/tar.gz/refs/tags/8.1.0",
        ],
    )

def _com_github_nelhage_rules_boost():
    # use boost 1.83
    RULES_BOOST_COMMIT = "cfa585b1b5843993b70aa52707266dc23b3282d0"
    maybe(
        http_archive,
        name = "com_github_nelhage_rules_boost",
        sha256 = "a7c42df432fae9db0587ff778d84f9dc46519d67a984eff8c79ae35e45f277c1",
        strip_prefix = "rules_boost-%s" % RULES_BOOST_COMMIT,
        patch_args = ["-p1"],
        patches = ["@sf_serving//bazel:patches/rules_boost.patch"],
        urls = [
            "https://github.com/nelhage/rules_boost/archive/%s.tar.gz" % RULES_BOOST_COMMIT,
        ],
    )

def _com_github_facebook_zstd():
    maybe(
        http_archive,
        name = "com_github_facebook_zstd",
        build_file = "@sf_serving//bazel:zstd.BUILD",
        strip_prefix = "zstd-1.5.0",
        sha256 = "5194fbfa781fcf45b98c5e849651aa7b3b0a008c6b72d4a0db760f3002291e94",
        type = ".tar.gz",
        urls = [
            "https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz",
        ],
    )

def _com_github_google_snappy():
    maybe(
        http_archive,
        name = "com_github_google_snappy",
        sha256 = "75c1fbb3d618dd3a0483bff0e26d0a92b495bbe5059c8b4f1c962b478b6e06e7",
        strip_prefix = "snappy-1.1.9",
        build_file = "@sf_serving//bazel:snappy.BUILD",
        urls = [
            "https://github.com/google/snappy/archive/refs/tags/1.1.9.tar.gz",
        ],
    )

def _com_github_google_brotli():
    maybe(
        http_archive,
        name = "brotli",
        build_file = "@sf_serving//bazel:brotli.BUILD",
        sha256 = "f9e8d81d0405ba66d181529af42a3354f838c939095ff99930da6aa9cdf6fe46",
        strip_prefix = "brotli-1.0.9",
        urls = [
            "https://github.com/google/brotli/archive/refs/tags/v1.0.9.tar.gz",
        ],
    )

def _com_github_tencent_rapidjson():
    maybe(
        http_archive,
        name = "com_github_tencent_rapidjson",
        sha256 = "bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e",
        strip_prefix = "rapidjson-1.1.0",
        build_file = "@sf_serving//bazel:rapidjson.BUILD",
        urls = [
            "https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz",
        ],
    )

def _org_apache_thrift():
    maybe(
        http_archive,
        name = "org_apache_thrift",
        build_file = "@sf_serving//bazel:thrift.BUILD",
        sha256 = "5da60088e60984f4f0801deeea628d193c33cec621e78c8a43a5d8c4055f7ad9",
        strip_prefix = "thrift-0.13.0",
        urls = [
            "https://github.com/apache/thrift/archive/v0.13.0.tar.gz",
        ],
    )

def _org_apache_arrow():
    maybe(
        http_archive,
        name = "org_apache_arrow",
        sha256 = "07cdb4da6795487c800526b2865c150ab7d80b8512a31793e6a7147c8ccd270f",
        strip_prefix = "arrow-apache-arrow-14.0.2",
        build_file = "@sf_serving//bazel:arrow.BUILD",
        urls = [
            "https://github.com/apache/arrow/archive/refs/tags/apache-arrow-14.0.2.tar.gz",
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
        sha256 = "d475978da0cdc2d43b73f30910786759d593a9d8ee05b1b6846d1eb16c6d2e0c",
        strip_prefix = "pybind11-2.11.1",
        urls = [
            "https://github.com/pybind/pybind11/archive/refs/tags/v2.11.1.tar.gz",
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
