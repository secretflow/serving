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

# LIST=$(find ../secretflow_serving/apis -name "*.proto" | cut -c 4- | sort -t '\0' -n)
# echo ${LIST}

echo "1. Update spi doc."
docker run --rm -v $(pwd)/source/reference/:/out \
  -v $(pwd)/..:/protos \
  pseudomuto/protoc-gen-doc \
  --doc_opt=/out/spi_md.tmpl,spi.md $(find ../secretflow_serving/spis -name "*.proto" | cut -c 4- | sort -t '\0' -n) secretflow_serving/protos/feature.proto

echo "2. Update api doc."
docker run --rm -v $(pwd)/source/reference/:/out \
  -v $(pwd)/..:/protos \
  pseudomuto/protoc-gen-doc \
  --doc_opt=/out/api_md.tmpl,api.md $(find ../secretflow_serving/apis -name "*.proto" | cut -c 4- | sort -t '\0' -n) secretflow_serving/protos/feature.proto

echo "3. Update config doc."
docker run --rm -v $(pwd)/source/reference/:/out \
  -v $(pwd)/..:/protos \
  pseudomuto/protoc-gen-doc \
  --doc_opt=/out/config_md.tmpl,config.md $(find ../secretflow_serving/config -name "*.proto" | cut -c 4- | sort -t '\0' -n)

echo "4. Update model graph doc."
docker run --rm -v $(pwd)/source/reference/:/out \
  -v $(pwd)/..:/protos \
  pseudomuto/protoc-gen-doc \
  --doc_opt=/out/model_md.tmpl,model.md secretflow_serving/protos/attr.proto secretflow_serving/protos/op.proto secretflow_serving/protos/graph.proto secretflow_serving/protos/bundle.proto secretflow_serving/protos/data_type.proto secretflow_serving/protos/compute_trace.proto secretflow_serving/protos/compute_trace.proto

echo "5. Update span info doc."
docker run --rm -v $(pwd)/source/reference/:/out \
  -v $(pwd)/..:/protos \
  pseudomuto/protoc-gen-doc \
  --doc_opt=/out/span_info_md.tmpl,span_info.md secretflow_serving/server/trace/span_info.proto
