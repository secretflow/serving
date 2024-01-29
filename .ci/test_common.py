#! python3

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

import csv
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import time
from dataclasses import dataclass
from typing import Any, Dict, List

from google.protobuf.json_format import MessageToJson

import pyarrow as pa
from google.protobuf.json_format import MessageToJson

from secretflow_serving_lib import get_op
from secretflow_serving_lib.attr_pb2 import (
    AttrValue,
    DoubleList,
    StringList,
    Int32List,
    BoolList,
)
from secretflow_serving_lib.bundle_pb2 import FileFormatType, ModelBundle, ModelManifest
from secretflow_serving_lib.feature_pb2 import (
    Feature,
    FeatureField,
    FeatureParam,
    FeatureValue,
    FieldType,
)
from secretflow_serving_lib.graph_pb2 import (
    GraphDef,
    NodeDef,
)
from secretflow_serving_lib.link_function_pb2 import LinkFunctionType

# set up global env
g_script_name = os.path.abspath(sys.argv[0])
g_script_dir = os.path.dirname(g_script_name)
g_repo_dir = os.path.dirname(g_script_dir)
g_clean_up_service = True
g_clean_up_files = True


class ModelBuilder:
    def __init__(self, name, desc, graph_def: GraphDef):
        self.name = name
        self.desc = desc
        self.bundle = ModelBundle(name=name, desc=desc, graph=graph_def)

    def dump_tar_gz(self, path=".", filename=None):
        if filename is None:
            filename = "model.tar.gz"
        if not os.path.exists(path):
            os.makedirs(path, exist_ok=True)

        filename = os.path.join(path, filename)

        model_graph_filename = "model_graph.json"

        # dump manifest
        dump_pb_json_file(
            ModelManifest(
                bundle_path=model_graph_filename, bundle_format=FileFormatType.FF_JSON
            ),
            os.path.join(path, "MANIFEST"),
        )
        # dump model file
        dump_pb_json_file(self.bundle, os.path.join(path, model_graph_filename))

        with tarfile.open(filename, "w:gz") as model_tar:
            model_tar.add(os.path.join(path, "MANIFEST"), arcname="MANIFEST")
            model_tar.add(
                os.path.join(path, model_graph_filename), arcname=model_graph_filename
            )
            print(
                f'tar: {filename} <- ({os.path.join(path, "MANIFEST")}, {os.path.join(path, model_graph_filename)})'
            )
        os.remove(os.path.join(path, "MANIFEST"))
        os.remove(os.path.join(path, model_graph_filename))
        with open(filename, "rb") as ifile:
            return filename, hashlib.sha256(ifile.read()).hexdigest()


def make_processing_node_def(
    name,
    parents,
    input_schema: pa.Schema,
    output_schema: pa.Schema,
    trace_content=None,
):
    op_def = get_op("ARROW_PROCESSING")
    attrs = {
        "input_schema_bytes": AttrValue(by=input_schema.serialize().to_pybytes()),
        "output_schema_bytes": AttrValue(by=output_schema.serialize().to_pybytes()),
        "content_json_flag": AttrValue(b=True),
    }
    if trace_content:
        attrs["trace_content"] = AttrValue(by=trace_content)

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def make_dot_product_node_def(
    name, parents, weight_dict, output_col_name, input_types, intercept=None
):
    op_def = get_op("DOT_PRODUCT")
    attrs = {
        "feature_names": AttrValue(ss=StringList(data=list(weight_dict.keys()))),
        "feature_weights": AttrValue(ds=DoubleList(data=list(weight_dict.values()))),
        "output_col_name": AttrValue(s=output_col_name),
        "input_types": AttrValue(ss=StringList(data=input_types)),
    }
    if intercept:
        attrs["intercept"] = AttrValue(d=intercept)

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def make_merge_y_node_def(
    name,
    parents,
    link_function: LinkFunctionType,
    input_col_name: str,
    output_col_name: str,
    yhat_scale: float = None,
):
    op_def = get_op("MERGE_Y")
    attrs = {
        "link_function": AttrValue(s=LinkFunctionType.Name(link_function)),
        "input_col_name": AttrValue(s=input_col_name),
        "output_col_name": AttrValue(s=output_col_name),
    }
    if yhat_scale:
        attrs["yhat_scale"] = AttrValue(d=yhat_scale)

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def make_tree_select_node_def(
    name,
    parents,
    root_node_id,
    tree_nodes: Dict,
    output_col_name,
    feature_dict: Dict[str, str],
):
    op_def = get_op("TREE_SELECT")
    attrs = {
        "input_feature_names": AttrValue(ss=StringList(data=list(feature_dict.keys()))),
        "input_feature_types": AttrValue(
            ss=StringList(data=list(feature_dict.values()))
        ),
        "output_col_name": AttrValue(s=output_col_name),
        "root_node_id": AttrValue(i32=root_node_id),
        "node_ids": AttrValue(
            i32s=Int32List(
                data=[node['nodeId'] for node in tree_nodes if 'nodeId' in node]
            )
        ),
        "lchild_ids": AttrValue(
            i32s=Int32List(
                data=[node['lchildId'] for node in tree_nodes if 'lchildId' in node]
            )
        ),
        "rchild_ids": AttrValue(
            i32s=Int32List(
                data=[node['rchildId'] for node in tree_nodes if 'rchildId' in node]
            )
        ),
        "leaf_flags": AttrValue(
            bs=BoolList(
                data=[node['isLeaf'] for node in tree_nodes if 'isLeaf' in node]
            )
        ),
        "split_feature_idxs": AttrValue(
            i32s=Int32List(
                data=[
                    node['splitFeatureIdx']
                    for node in tree_nodes
                    if 'splitFeatureIdx' in node
                ]
            )
        ),
        "split_values": AttrValue(
            ds=DoubleList(
                data=[node['splitValue'] for node in tree_nodes if 'splitValue' in node]
            )
        ),
    }

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def make_tree_merge_node_def(
    name,
    parents,
    input_col_name,
    output_col_name,
    leaf_node_weights: Dict[int, float] = None,
):
    op_def = get_op("TREE_MERGE")
    attrs = {
        "input_col_name": AttrValue(s=input_col_name),
        "output_col_name": AttrValue(s=output_col_name),
    }
    if leaf_node_weights:
        attrs['leaf_node_ids'] = AttrValue(
            i32s=Int32List(data=list(leaf_node_weights.keys()))
        )
        attrs['leaf_weights'] = AttrValue(
            ds=DoubleList(data=list(leaf_node_weights.values()))
        )

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def make_tree_ensemble_predict_node_def(
    name,
    parents,
    input_col_name,
    output_col_name,
    num_trees,
    func_type,
):
    op_def = get_op("TREE_ENSEMBLE_PREDICT")
    attrs = {
        "input_col_name": AttrValue(s=input_col_name),
        "output_col_name": AttrValue(s=output_col_name),
        "num_trees": AttrValue(i32=num_trees),
        "func_type": AttrValue(s=func_type),
    }

    return NodeDef(
        name=name,
        parents=parents,
        op=op_def.name,
        attr_values=attrs,
        op_version=op_def.version,
    )


def dump_pb_json_file(pb_obj, file_name, indent=2):
    json_str = MessageToJson(pb_obj)
    with open(file_name, "w") as file:
        file.write(json_str)


def dump_json(obj, filename, indent=2):
    with open(filename, "w") as ofile:
        json.dump(obj, ofile, indent=indent)


@dataclass
class PartyConfig:
    id: str
    feature_mapping: Dict[str, str]
    cluster_ip: str
    metrics_port: int
    brpc_builtin_service_port: int
    channel_protocol: str
    model_id: str
    graph_def: GraphDef
    query_datas: List[str] = None
    query_context: str = None
    csv_dict: Dict[str, Any] = None


class ConfigDumper:
    def __init__(
        self,
        party_configs: List[PartyConfig],
        log_config_filename: str,
        serving_config_filename: str,
        tar_name: str,
        service_id: str,
    ):
        self.service_id = service_id
        self.party_configs = party_configs
        self.parties = []
        self.log_config = log_config_filename
        self.serving_config = serving_config_filename
        self.tar_name = tar_name
        for config in self.party_configs:
            self.parties.append({"id": config.id, "address": config.cluster_ip})

    def _dump_logging_config(self, path: str, logging_path: str):
        with open(os.path.join(path, self.log_config), "w") as ofile:
            json.dump({"systemLogPath": os.path.abspath(logging_path)}, ofile, indent=2)

    def _dump_model_tar_gz(self, path: str, graph_def: GraphDef):
        graph_def_str = MessageToJson(graph_def, preserving_proto_field_name=True)
        print(f"graph_def: \n {graph_def_str}")
        return ModelBuilder("test_model", "just for test", graph_def).dump_tar_gz(
            path, self.tar_name
        )

    def make_csv_config(self, data_dict: Dict[str, List[Any]], path: str):
        filename = "feature_source.csv"
        file_path = os.path.join(path, filename)
        with open(file_path, "w") as ofile:
            writer = csv.DictWriter(ofile, fieldnames=list(data_dict.keys()))
            writer.writeheader()
            rows = []
            for key, value in data_dict.items():
                if len(rows) == 0:
                    rows = [{} for _ in value]
                assert len(value) == len(
                    rows
                ), f"row count {len(value)} of {key} in data_dict is diff with {len(rows)}."
                for i in range(len(value)):
                    rows[i][key] = value[i]
            print("CSV Rows: ", rows)
            for row in rows:
                writer.writerow(row)
        return {"csv_opts": {"file_path": file_path, "id_name": "id"}}

    def _dump_serving_config(
        self, path: str, config: PartyConfig, model_name: str, model_sha256: str
    ):
        config_dict = {
            "id": self.service_id,
            "serverConf": {
                "featureMapping": config.feature_mapping,
                "metricsExposerPort": config.metrics_port,
                "brpcBuiltinServicePort": config.brpc_builtin_service_port,
            },
            "modelConf": {
                "modelId": config.model_id,
                "basePath": os.path.abspath(path),
                "sourcePath": os.path.abspath(model_name),
                "sourceSha256": model_sha256,
                "sourceType": "ST_FILE",
            },
            "clusterConf": {
                "selfId": config.id,
                "parties": self.parties,
                "channel_desc": {"protocol": config.channel_protocol},
            },
            "featureSourceConf": self.make_csv_config(config.csv_dict, path)
            if config.csv_dict
            else {"mockOpts": {}},
        }
        dump_json(config_dict, os.path.join(path, self.serving_config))

    def dump(self, path="."):
        for config in self.party_configs:
            config_path = os.path.join(path, config.id)
            if not os.path.exists(config_path):
                os.makedirs(config_path, exist_ok=True)
            self._dump_logging_config(config_path, os.path.join(config_path, "log"))
            model_name, model_sha256 = self._dump_model_tar_gz(
                config_path, config.graph_def
            )
            self._dump_serving_config(config_path, config, model_name, model_sha256)


# for every testcase, there should be a TestConfig instance
class TestConfig:
    def __init__(
        self,
        model_path: str,
        party_config: List[PartyConfig],
        header_dict: Dict[str, str] = None,
        service_spec_id: str = None,
        predefined_features: Dict[str, List[Any]] = None,
        predefined_types: Dict[str, str] = None,
        log_config_name=None,
        serving_config_name=None,
        tar_name=None,
        specific_party=None,
    ):
        self.header_dict = header_dict
        self.service_spec_id = service_spec_id
        self.predefined_features = predefined_features
        self.predefined_types = predefined_types
        self.model_path = os.path.join(g_script_dir, model_path)
        self.party_config = party_config
        self.log_config_name = (
            log_config_name if log_config_name is not None else "logging.config"
        )
        self.serving_config_name = (
            serving_config_name if serving_config_name is not None else "serving.config"
        )
        self.tar_name = tar_name if tar_name is not None else "model.tar.gz"
        self.background_proc = []
        self.specific_party = specific_party

    def dump_config(self):
        ConfigDumper(
            self.party_config,
            self.log_config_name,
            self.serving_config_name,
            self.tar_name,
            self.service_spec_id,
        ).dump(self.model_path)

    def get_server_start_args(self):
        def merge_path(dir, party_id, filename):
            return os.path.abspath(os.path.join(dir, party_id, filename))

        return [
            f"--serving_config_file={merge_path(self.model_path, config.id, self.serving_config_name)} "
            f"--logging_config_file={merge_path(self.model_path, config.id, self.log_config_name)} "
            for config in self.party_config
        ]

    def get_party_ids(self):
        return [config.id for config in self.party_config]

    def make_request(self):
        if self.predefined_features:
            pre_features = []
            for name, data_list in self.predefined_features.items():
                pre_features.append(
                    make_feature(name, data_list, self.predefined_types[name])
                )
        else:
            pre_features = None

        if self.party_config[0].query_datas:
            fs_param = {}
            for config in self.party_config:
                fs_param[config.id] = FeatureParam(
                    query_datas=config.query_datas, query_context=config.query_context
                )
        else:
            fs_param = None

        return PredictRequest(
            self.header_dict, self.service_spec_id, fs_param, pre_features
        )

    def make_model_info_request(self):
        body_dict = {"service_spec": {"id": self.service_spec_id}}
        return json.dumps(body_dict)

    def make_predict_curl_cmd(self, party: str):
        url = None
        for p_cfg in self.party_config:
            if p_cfg.id == party:
                url = f"http://{p_cfg.cluster_ip}/PredictionService/Predict"
                break
        if not url:
            raise Exception(
                f"{party} is not in TestConfig({self.config.get_party_ids()})"
            )
        curl_wrapper = CurlWrapper(
            url=url,
            header="Content-Type: application/json",
            data=self.make_request().to_json(),
        )
        return curl_wrapper.cmd()

    def make_get_model_info_curl_cmd(self, party: str):
        url = None
        for p_cfg in self.party_config:
            if p_cfg.id == party:
                url = f"http://{p_cfg.cluster_ip}/ModelService/GetModelInfo"
                break
        if not url:
            raise Exception(
                f"{party} is not in TestConfig({self.config.get_party_ids()})"
            )
        curl_wrapper = CurlWrapper(
            url=url,
            header="Content-Type: application/json",
            data=self.make_model_info_request(),
        )
        return curl_wrapper.cmd()

    def _exe_cmd(self, cmd, background=False):
        print("Execute: ", cmd)
        if not background:
            ret = subprocess.run(cmd, shell=True, check=True, capture_output=True)
            ret.check_returncode()
            return ret
        else:
            proc = subprocess.Popen(cmd.split(), shell=False)
            self.background_proc.append(proc)
            return proc

    def finish(self):
        if g_clean_up_service:
            for proc in self.background_proc:
                proc.kill()
                proc.wait()
        if g_clean_up_files:
            os.system(f"rm -rf {self.model_path}")

    def exe_start_server_scripts(self, start_interval_s=0):
        for arg in self.get_server_start_args():
            self._exe_cmd(
                f"./bazel-bin/secretflow_serving/server/secretflow_serving {arg}", True
            )
            if start_interval_s:
                time.sleep(start_interval_s)

        # wait 10s for servers be ready
        time.sleep(10)

    def exe_curl_request_scripts(self, party: str):
        return self._exe_cmd(self.make_predict_curl_cmd(party))

    def exe_get_model_info_request_scripts(self, party: str):
        return self._exe_cmd(self.make_get_model_info_curl_cmd(party))


def make_feature(name: str, value: List[Any], f_type: str):
    assert len(value) != 0

    field_type = FieldType.Value(f_type)

    if field_type == FieldType.FIELD_BOOL:
        f_value = FeatureValue(bs=[bool(v) for v in value])
    elif field_type == FieldType.FIELD_FLOAT:
        f_value = FeatureValue(fs=[float(v) for v in value])
    elif field_type == FieldType.FIELD_DOUBLE:
        f_value = FeatureValue(ds=[float(v) for v in value])
    elif field_type == FieldType.FIELD_INT32:
        f_value = FeatureValue(i32s=[int(v) for v in value])
    elif field_type == FieldType.FIELD_INT64:
        f_value = FeatureValue(i64s=[int(v) for v in value])
    else:
        f_value = FeatureValue(ss=[str(v) for v in value])

    return Feature(field=FeatureField(name=name, type=field_type), value=f_value)


class PredictRequest:
    def __init__(
        self,
        header_dict: Dict[str, str] = None,
        service_spec_id: str = None,
        party_param_dict: Dict[str, FeatureParam] = None,
        predefined_feature: List[Feature] = None,
    ):
        self.header_dict = header_dict
        self.service_spec_id = service_spec_id
        self.party_param_dict = party_param_dict
        self.predefined_feature = predefined_feature

    def to_json(self):
        ret = {}
        if self.header_dict:
            ret["header"] = {"data": self.header_dict}
        if self.service_spec_id:
            ret["service_spec"] = {"id": self.service_spec_id}
        if self.party_param_dict:
            ret["fs_params"] = {
                k: json.loads(MessageToJson(v, preserving_proto_field_name=True))
                for k, v in self.party_param_dict.items()
            }
        if self.predefined_feature:
            ret["predefined_features"] = [
                json.loads(MessageToJson(i, preserving_proto_field_name=True))
                for i in self.predefined_feature
            ]
        return json.dumps(ret)


class CurlWrapper:
    def __init__(self, url: str, header: str, data: str):
        self.url = url
        self.header = header
        self.data = data

    def cmd(self):
        return f'curl --location "{self.url}" --header "{self.header}" --data \'{self.data}\''

    def exe(self):
        return os.popen(self.cmd())


# base class
class TestCase:
    def __init__(self, path: str):
        self.path = path

    def exec(self):
        config = self.get_config(self.path)
        try:
            self.test(config)
        finally:
            config.finish()

    def test(config: TestConfig):
        raise NotImplementedError

    def get_config(self, path: str) -> TestConfig:
        raise NotImplementedError
