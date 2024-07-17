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
    _com_github_opentelemetry_cpp()

    # aws s3
    _com_aws_c_common()
    _com_aws_c_event_stream()
    _com_aws_checksums()
    _com_aws_sdk()
    _com_github_curl()
    _kuscia()
    _yacl()

    _com_github_brpc_brpc()

def _yacl():
    maybe(
        http_archive,
        name = "yacl",
        urls = [
            "https://github.com/secretflow/yacl/archive/refs/tags/0.4.5b1.tar.gz",
        ],
        strip_prefix = "yacl-0.4.5b1",
        sha256 = "28064053b9add0db8e1e8e648421a0579f1d3e7ee8a4bbd7bd5959cb59598088",
    )

def _kuscia():
    maybe(
        http_archive,
        name = "kuscia",
        urls = [
            "https://github.com/secretflow/kuscia/archive/refs/tags/v0.5.0b0.tar.gz",
        ],
        strip_prefix = "kuscia-0.5.0b0",
        sha256 = "6db0a23dbaf4a1fc223acc51425daa1572126410fb583f3132b7a1ade985e934",
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
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
        ],
        sha256 = "218efe8ee736d26a3572663b374a253c012b716d8af0c07e842e82f238a0a7ee",
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
            "https://github.com/awslabs/aws-c-common/archive/v0.7.6.tar.gz",
        ],
        sha256 = "2253bddca034108266f983f60a76a704e7dc71a94700ee2d8411da74adbaa62d",
        strip_prefix = "aws-c-common-0.7.6",
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
        strip_prefix = "prometheus-cpp-1.1.0",
        sha256 = "397544fe91e183029120b4eebcfab24ed9ec833d15850aae78fd5db19062d13a",
        urls = [
            "https://github.com/jupp0r/prometheus-cpp/archive/refs/tags/v1.1.0.tar.gz",
        ],
    )

def _com_github_opentelemetry_cpp():
    maybe(
        http_archive,
        name = "io_opentelemetry_cpp",
        strip_prefix = "opentelemetry-cpp-1.14.2",
        sha256 = "c7e7801c9f6228751cdb9dd4724d0f04777ed53f524c8828e73bf4c9f894e0bd",
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.14.2.tar.gz",
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
        sha256 = "04ec44461850abbf33824da84978043b22554896b552c5fd11a9c5ae4b4d296e",
        strip_prefix = "double-conversion-3.3.0",
        build_file = "@sf_serving//bazel:double_conversion.BUILD",
        urls = [
            "https://github.com/google/double-conversion/archive/refs/tags/v3.3.0.tar.gz",
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
        strip_prefix = "zstd-1.5.6",
        sha256 = "8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1",
        type = ".tar.gz",
        urls = [
            "https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz",
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
        sha256 = "e720a6ca29428b803f4ad165371771f5398faba397edf6778837a18599ea13ff",
        strip_prefix = "brotli-1.1.0",
        urls = [
            "https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz",
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
        sha256 = "cd7b829d3d9d87f9f7d708e004eef7629789591ee1d416f4741913bc33e5c27d",
        strip_prefix = "thrift-0.20.0",
        urls = [
            "https://github.com/apache/thrift/archive/v0.20.0.tar.gz",
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

# serving not use brpc x-bd-xxx trace header, so drop the patch of yacl
# add for brpc compile
def _com_github_brpc_brpc():
    maybe(
        http_archive,
        name = "com_github_brpc_brpc",
        sha256 = "13ffb2f1f57c679379a20367c744b3e597614a793ec036cd7580aae90798019d",
        strip_prefix = "brpc-1.8.0",
        type = "tar.gz",
        patch_args = ["-p1"],
        patches = [
            "@yacl//bazel:patches/brpc_m1.patch",
            "@yacl//bazel:patches/brpc_pb.patch",
            "@sf_serving//bazel:patches/brpc.patch",
        ],
        urls = [
            "https://github.com/apache/brpc/archive/refs/tags/1.8.0.tar.gz",
        ],
    )
