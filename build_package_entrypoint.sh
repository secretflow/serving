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

bazel build --verbose_failures -c opt //:sf_serving

cp bazel-bin/sf_serving.tar.gz ./
