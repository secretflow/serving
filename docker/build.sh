#!/bin/bash
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

set -eu

RED='\033[0;31m'
GREEN="\033[32m"
NO_COLOR="\033[0m"

show_help() {
    echo "Usage: bash build.sh [OPTION]"
    echo "  -v version"
    echo "          specifies the image version. default use binary version"
    echo "  -l"
    echo "          tag this version as latest."
    echo "  -p"
    echo "          upload the builded image to a registry."
    echo
}

MACHINE_TYPE=$(arch)
HOST_PLATFORM=""
RELEASE_IMAGE=""
if [ "$MACHINE_TYPE" == "x86_64" ]; then
  HOST_PLATFORM=linux/amd64
  RELEASE_IMAGE=secretflow/release-ci:latest
else
  HOST_PLATFORM=linux/arm64
  RELEASE_IMAGE=secretflow/release-ci-aarch64:latest
fi

CODE_PATH=$(cd "$(dirname "$0")" && cd .. && pwd)

build_serving() {
    echo -e "${GREEN}Start build serving package...${NO_COLOR} ${CODE_PATH}"

    # prepare version information before build
    docker run --rm --mount type=bind,source="${CODE_PATH}",target=/home/admin/dev/ \
        -w /home/admin/dev \
        --entrypoint "./build_package_entrypoint.sh" \
        ${RELEASE_IMAGE}

    (($? != 0)) && {
        echo -e "${RED}Build exited with non-zero.${NO_COLOR}"
        exit 1
    }

    echo -e "${GREEN}Build serving package end${NO_COLOR}"
}

VERSION=""
LATEST=0
NEED_PUSH=0
while getopts "v:pl" options; do
  case "${options}" in
  v)
    VERSION=${OPTARG}
    if [[ -z ${VERSION} ]]; then
        VERSION=$(grep "version" $CODE_PATH/version.txt | awk -F'"' '{print $2}')
        echo "Version shall not be empty."
        exit 1
    fi
    ;;
  l)
    LATEST=1
    ;;
  p)
    NEED_PUSH=1
    ;;
  *)
    show_help
    exit 1
    ;;
  esac
done

build_serving

if [[ -z ${VERSION} ]]; then
    VERSION=$(grep "version" $CODE_PATH/version.txt | awk -F'"' '{print $2}')
    echo "use binary version($VERSION) as docker image tag"
fi

IMAGE_TAG=registry.cn-shanghai.aliyuncs.com/inscar-opensource-stack/serving:${VERSION}
LATEST_TAG=registry.cn-shanghai.aliyuncs.com/inscar-opensource-stack/serving:latest

# copy package
mkdir -p ${HOST_PLATFORM}
cp ${CODE_PATH}/sf_serving.tar.gz ./${HOST_PLATFORM}/
cp ${CODE_PATH}/docker/logging.config ./${HOST_PLATFORM}/

echo -e "Building ${GREEN}${IMAGE_TAG}${NO_COLOR}"
docker build --build-arg="TARGETPLATFORM=${HOST_PLATFORM}" . -f Dockerfile -t ${IMAGE_TAG}
echo -e "Finish building ${GREEN}${IMAGE_TAG}${NO_COLOR}"

if [[ LATEST -eq 1 ]]; then
    echo -e "Tag ${GREEN}${LATEST_TAG}${NO_COLOR} ..."
    docker tag ${IMAGE_TAG} ${LATEST_TAG}
fi

if [[ NEED_PUSH -eq 1 ]]; then
    echo -e "Push ${GREEN}${IMAGE_TAG}${NO_COLOR}"
    docker push ${IMAGE_TAG}
    if [[ LATEST -eq 1 ]]; then
      echo -e "Push ${GREEN}${LATEST_TAG}${NO_COLOR}"
      docker push ${LATEST_TAG}
    fi
fi

# clean up
rm -rf ./${HOST_PLATFORM}
