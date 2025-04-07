#! /bin/bash
#
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
#

BASE_DIR=$(cd "$(dirname "$0")" && pwd)

show_help() {
    echo "Usage: bash build_package_entrypoint.sh [OPTION]"
    echo "  -j bazel_jobs"
    echo "          specifies the limit number of bazel jobs."
    echo
}

bazel_jobs=0
while getopts "j:" options; do
  case "${options}" in
  j)
    bazel_jobs=${OPTARG}
    if [[ ${bazel_jobs} -le 0 ]]; then
        echo "-j value shall be greater than 0, get ${OPTARG}."
        exit 1
    fi
    ;;
  *)
    show_help
    exit 1
    ;;
  esac
done

source ${BASE_DIR}/update_version.sh

if [[ ${bazel_jobs} -le 0 ]]; then
    bazel build --verbose_failures -c opt //:sf_serving
else
    bazel build --verbose_failures -c opt //:sf_serving --jobs ${bazel_jobs}
fi

cp bazel-bin/sf_serving.tar.gz ${BASE_DIR}/
