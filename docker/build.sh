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

set -e

RED='\033[0;31m'
GREEN="\033[32m"
NO_COLOR="\033[0m"

show_help() {
    echo "Usage: bash build.sh [OPTION]... -v {the_version}"
    echo "  -v  --version"
    echo "          the version to build with."
    echo "  -l --latest"
    echo "          tag this version as latest and push to docker repo."
    echo
}

RELEASE_IMAGE=secretflow/release-ci:1.2

CODE_PATH=$(cd .. && pwd)
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

build_serving() {
    echo -e "${GREEN}Start build serving package...${NO_COLOR} ${CODE_PATH}"

    # prepare version information before build
    version=$(grep "version" $SCRIPT_DIR/version.txt | awk -F'"' '{print $2}')
    version+=$(date '+%Y%m%d-%H:%M:%S')
    version+=".$(git rev-parse --short HEAD)"
    echo "binary version: ${version}"
    sed -i "s/SF_SERVING_VERSION/${version}/g" ${CODE_PATH}/secretflow_serving/server/version.h

    docker run --rm --mount type=bind,source="${CODE_PATH}",target=/home/admin/dev/ \
        -w /home/admin/dev \
        --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
        --cap-add=NET_ADMIN \
        --privileged=true \
        --entrypoint "./build_package_entrypoint.sh" \
        ${RELEASE_IMAGE}

    (($? != 0)) && {
        echo -e "${RED}Build exited with non-zero.${NO_COLOR}"
        exit 1
    }

    echo -e "${GREEN}Build serving package end${NO_COLOR}"
}

if [[ "$#" -lt 2 ]]; then
    show_help
    exit
fi

while [[ "$#" -ge 1 ]]; do
    case $1 in
    -v | --version)
        VERSION="$2"
        shift
        if [[ "$#" -eq 0 ]]; then
            echo "Version shall not be empty."
            echo ""
            show_help
            exit 1
        fi
        shift
        ;;
    -l | --latest)
        LATEST=1
        shift
        ;;
    *)
        echo "Unknown argument passed: $1"
        exit 1
        ;;
    esac
done

if [[ -z ${VERSION} ]]; then
    echo "Please specify the version."
    exit 1
fi

IMAGE_TAG=secretflow/serving-anolis8:${VERSION}
LATEST_TAG=secretflow/serving-anolis8:latest

build_serving

# copy package
cp ${CODE_PATH}/sf_serving.tar.gz ./

echo -e "Building ${GREEN}${IMAGE_TAG}${NO_COLOR}"
docker build . -f Dockerfile -t ${IMAGE_TAG}
echo -e "Finish building ${GREEN}${IMAGE_TAG}${NO_COLOR}"
docker push ${IMAGE_TAG}

if [[ LATEST -eq 1 ]]; then
    echo -e "Tag and push ${GREEN}${LATEST_TAG}${NO_COLOR} ..."
    docker tag ${IMAGE_TAG} ${LATEST_TAG}
    docker push ${LATEST_TAG}
fi

# clean up
rm -f sf_serving.tar.gz
