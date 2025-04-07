#! /bin/bash
#
# Copyright 2025 Ant Group Co., Ltd.
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
version=$(grep "SERVING_VERSION" $BASE_DIR/version.bzl | awk -F'"' '{print $2}')
version_key=$(grep "SERVING_VERSION" "$BASE_DIR/version.bzl" | awk -F'"' '{print $1}')

new_version=$version
# Check if the version contains '$$DATE$$' and replace it
if [[ $version == *'dev' ]]; then
    new_version+=$(date '+%Y%m%d')

    # Using sed to write the new version back to the file
    sed -i "s/${version_key}\"${version}\"/${version_key}\"${new_version}\"/" "$BASE_DIR/version.bzl"
fi
echo "binary version: ${new_version}"
sed -i "s/SF_SERVING_VERSION/${new_version}/g" ${BASE_DIR}/secretflow_serving/server/version.h
