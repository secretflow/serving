# Copyright 2024 Ant Group Co., Ltd.
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


set -eu

yum install iproute-tc -y;
# tc qdisc add dev eth0 root handle 1: tbf rate 100mbit burst 128kb latency 10ms;
# tc qdisc add dev eth0 parent 1:1 handle 10: netem delay 10msec limit 8000

