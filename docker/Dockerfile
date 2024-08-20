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

FROM openanolis/anolisos:8.8

ARG TARGETPLATFORM

ENV TZ=Asia/Shanghai

LABEL maintainer="secretflow-contact@service.alipay.com"

RUN yum install -y protobuf libnl3 libgomp && yum clean all

COPY ./$TARGETPLATFORM/sf_serving.tar.gz /root/sf_serving.tgz
RUN tar -C /root -xzf /root/sf_serving.tgz

# run as root for now
WORKDIR /root/sf_serving

CMD ["/bin/bash"]
