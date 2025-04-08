#! /bin/bash
#
# Copyright 2022 Ant Group Co., Ltd.
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
set -e

show_help() {
    echo "Usage: bash build_wheel_entrypoint.sh [OPTION]"
    echo "  -j bazel_jobs"
    echo "          specifies the limit number of bazel jobs."
    echo "  -p pytho_version"
    echo "          specifies the python version."
    echo
}

BASE_DIR=$(cd "$(dirname "$0")" && pwd)

bazel_jobs=0
python_ver="3.10"
while getopts "j:p:" options; do
    case "${options}" in
    j)
        bazel_jobs=${OPTARG}
        if [[ ${bazel_jobs} -le 0 ]]; then
            echo "-j value shall be greater than 0, get ${OPTARG}."
            exit 1
        fi
        ;;
    p)
        python_ver=${OPTARG}
        ;;
    *)
        show_help
        exit 1
        ;;
    esac
done

python3 ${BASE_DIR}/update_version.py

if [[ ${bazel_jobs} -le 0 ]]; then
    bazel build --verbose_failures -c opt //:serving_lib --@rules_python//python/config_settings:python_version=${python_ver}
else
    bazel build --verbose_failures -c opt //:serving_lib --@rules_python//python/config_settings:python_version=${python_ver} --jobs ${bazel_jobs}
fi

python -m pip install ${BASE_DIR}/bazel-bin/*.whl --force-reinstall
