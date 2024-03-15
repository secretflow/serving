#!/bin/bash
#
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
#

set -ex

port=9998
script_dir=$(realpath $(dirname $0))
csv_filename=${script_dir}/test.csv
id_column_name=id

if [ -n "$(ps -ef | grep -v grep | grep simple_feature_service/simple_feature_service)" ]; then
    killall  simple_feature_service
fi

cd $script_dir
cd ../../../
bazel build -c dbg  //secretflow_serving/tools/simple_feature_service/...

./bazel-bin/secretflow_serving/tools/simple_feature_service/simple_feature_service \
    --port=$port \
    --csv_filename=$csv_filename \
    --csv_id_column_name=$id_column_name &

sleep 2

curl --location "http://127.0.0.1:${port}/BatchFeatureService/BatchFetchFeature" \
    --header 'Content-Type: application/json' \
    --data  '{"header": {"data": {"custom_str": "id_12345"}}, "model_service_id": "test_service_id", "party_id": "alice", "feature_fields": [{"name": "a", "type": "FIELD_INT64"}, {"name": "b", "type": "FIELD_DOUBLE"},{"name": "c", "type": "FIELD_STRING"}], "param": {"query_datas": ["c1", "c2"], "query_context": "context_x"}}'

killall  simple_feature_service