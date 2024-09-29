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

# prepare version information before build
version=$(grep "__version__" $BASE_DIR/version.txt | awk -F'"' '{print $2}')
version_key=$(grep "__version__" "$BASE_DIR/version.txt" | awk -F'"' '{print $1}')

new_version=$version
# Check if the version contains '$$DATE$$' and replace it
if [[ $version == *'$$DATE$$'* ]]; then
    date_string=$(date '+%Y%m%d')
    new_version=${version//'$$DATE$$'/$date_string}

    # Using sed to write the new version back to the file
    sed -i "s/${version_key}\"${version}\"/${version_key}\"${new_version}\"/" "$BASE_DIR/version.txt"
fi
echo "binary version: ${new_version}"
sed -i "s/SF_SERVING_VERSION/${new_version}/g" ${BASE_DIR}/secretflow_serving/server/version.h

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

# OPENSOURCE-CLEANUP GSUB REMOVE KEYWORD_ONLY --repository_cache=/tmp/bazel_repo_cache
if [[ ${bazel_jobs} -le 0 ]]; then
    bazel build --verbose_failures -c opt //:sf_serving --repository_cache=/tmp/bazel_repo_cache
else
    bazel build --verbose_failures -c opt //:sf_serving --jobs ${bazel_jobs} --repository_cache=/tmp/bazel_repo_cache
fi


cp bazel-bin/sf_serving.tar.gz ${BASE_DIR}/
