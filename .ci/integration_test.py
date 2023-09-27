#! python3

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


import csv
import hashlib
import json
import os
import subprocess
import sys
import tarfile
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, List

# set up global env
g_script_name = os.path.abspath(sys.argv[0])
g_script_dir = os.path.dirname(g_script_name)
g_repo_dir = os.path.dirname(g_script_dir)


class AttrValue:
    def __init__(self, value, type=None):
        assert value, "value cannot be null"
        self.value = value
        self.type = None

    def to_json(self):
        def make_dict(type, value):
            if isinstance(self.value, list):
                return {self.type: {"data": self.value}}
            else:
                return {self.type: self.value}

        if self.type:
            return make_dict(self.type, self.value)

        def get_attr_prefix(value):
            if isinstance(value, int):
                return "i32"
            elif isinstance(value, float):
                return "f"
            elif isinstance(value, bool):
                return "b"
            else:
                return "s"

        if isinstance(self.value, list):
            assert len(self.value) != 0, "list is None, cannont deduce type"
            type = get_attr_prefix(self.value[0])
            return make_dict(type + "s", self.value)
        else:
            return {get_attr_prefix(self.value): self.value}


class MapAttrValue:
    def __init__(self, attr_values: Dict[str, AttrValue]):
        self.data = attr_values

    def to_json(self):
        return {k: v.to_json() for k, v in self.data.items()}


class DispatchType(Enum):
    UNKNOWN_DP_TYPE = 0
    DP_ALL = 1
    DP_ANYONE = 2


class RuntimeConfig:
    def __init__(self, dispatch_type: Enum, session_run: bool):
        self.dispatch_type = dispatch_type
        self.session_run = session_run

    def to_json(self):
        return {
            "dispatch_type": self.dispatch_type.name,
            "session_run": self.session_run,
        }


class NodeDef:
    def __init__(self, name: str, op: str, parents: List[str], data_dict):
        self.name = name
        self.op = op
        self.parents = parents
        self.attr_values = trans_normal_dict_to_attr_dict(data_dict)

    def to_json(self):
        return {
            "name": self.name,
            "op": self.op,
            "parents": self.parents,
            "attr_values": self.attr_values.to_json(),
        }


class ExecutionDef:
    def __init__(self, nodes: List[str], config: RuntimeConfig):
        self.nodes = nodes
        self.config = config

    def to_json(self):
        return {"nodes": self.nodes, "config": self.config.to_json()}


class GraphDef:
    def __init__(
        self,
        version: str,
        node_list: List[NodeDef] = None,
        execution_list: List[ExecutionDef] = None,
    ):
        self.node_list = node_list if node_list else []
        self.execution_list = execution_list if execution_list else []
        self.version = version

    def to_json(self):
        return {
            "version": self.version,
            "node_list": [node.to_json() for node in self.node_list],
            "execution_list": [exe.to_json() for exe in self.execution_list],
        }


class AttrValue:
    def __init__(self, value, type=None):
        assert value, "value cannot be null"
        self.value = value
        self.type = None

    def to_json(self):
        def make_dict(type, value):
            if isinstance(value, list):
                return {type: {"data": value}}
            else:
                return {type: value}

        if self.type:
            return make_dict(self.type, self.value)

        def get_attr_prefix(value):
            if isinstance(value, int):
                return "i32"
            elif isinstance(value, float):
                return "d"
            elif isinstance(value, bool):
                return "b"
            else:
                return "s"

        if isinstance(self.value, list):
            assert len(self.value) != 0, "list is None, cannont deduce type"
            type = get_attr_prefix(self.value[0])
            return make_dict(type + "s", self.value)
        else:
            return {get_attr_prefix(self.value): self.value}


def trans_normal_dict_to_attr_dict(data_dict):
    ret = {}
    for k, v in data_dict.items():
        ret[k] = AttrValue(v)
    return MapAttrValue(ret)


class JsonModel:
    def __init__(self, name, desc, graph_def: GraphDef):
        self.name = name
        self.desc = desc
        self.graph_json = {"name": name, "desc": desc, "graph": graph_def.to_json()}

    def dump_tar_gz(self, path=".", filename=None):
        if filename is None:
            filename = "model.tar.gz"
        if not os.path.exists(path):
            os.makedirs(path, exist_ok=True)

        filename = os.path.join(path, filename)

        model_graph_filename = "model_graph.json"

        dump_json(
            {"bundle_path": model_graph_filename, "bundle_format": "FF_JSON"},
            os.path.join(path, "MANIFEST"),
        )

        dump_json(self.graph_json, os.path.join(path, model_graph_filename))

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
            return filename, hashlib.md5(ifile.read()).hexdigest()


class DotProcutAttr:
    def __init__(self, weight_dict: Dict[str, float], output_col_name, intercept=None):
        self.name = "DOT_PRODUCT"
        self.version = "0.0.1"
        self.desc = ""
        self.weight_dict = weight_dict
        self.feature_names = list(weight_dict.keys())
        self.feature_weights = list(weight_dict.values())
        self.output_col_name = output_col_name
        self.intercept = intercept

    def get_node_attr_map(self):
        assert self.feature_names
        assert self.feature_weights
        assert self.output_col_name
        ret = {
            "feature_names": self.feature_names,
            "feature_weights": self.feature_weights,
            "output_col_name": self.output_col_name,
        }
        if self.intercept:
            ret["intercept"] = self.intercept
        return ret


class LinkFunction(Enum):
    LF_LOG = 1
    LF_LOGIT = 2
    LF_INVERSE = 3
    LF_LOGIT_V2 = 4
    LF_RECIPROCAL = 5
    LF_INDENTITY = 6
    LF_SIGMOID_RAW = 11
    LF_SIGMOID_MM1 = 12
    LF_SIGMOID_MM3 = 13
    LF_SIGMOID_GA = 14
    LF_SIGMOID_T1 = 15
    LF_SIGMOID_T3 = 16
    LF_SIGMOID_T5 = 17
    LF_SIGMOID_T7 = 18
    LF_SIGMOID_T9 = 19
    LF_SIGMOID_LS7 = 20
    LF_SIGMOID_SEG3 = 21
    LF_SIGMOID_SEG5 = 22
    LF_SIGMOID_DF = 23
    LF_SIGMOID_SR = 24
    LF_SIGMOID_SEGLS = 25


class MergeYAttr:
    def __init__(
        self,
        link_function: LinkFunction,
        input_col_name: str,
        output_col_name: str,
        yhat_scale: float = None,
    ):
        self.name = "MERGE_Y"
        self.version = "0.0.1"
        self.desc = ""
        self.link_function = link_function
        self.yhat_scale = yhat_scale
        self.input_col_name = input_col_name
        self.output_col_name = output_col_name

    def get_node_attr_map(self):
        assert self.link_function
        assert self.input_col_name
        assert self.output_col_name
        ret = {
            "link_function": self.link_function.name,
            "input_col_name": self.input_col_name,
            "output_col_name": self.output_col_name,
        }
        if self.yhat_scale:
            ret["yhat_scale"] = self.yhat_scale
        return ret


def make_dot_product_node_def(
    name, parents, weight_dict, output_col_name, intercept=None
):
    dot_op = DotProcutAttr(weight_dict, output_col_name, intercept)
    return NodeDef(
        name, dot_op.name, parents=parents, data_dict=dot_op.get_node_attr_map()
    )


def make_merge_y_node_def(
    name,
    parents,
    link_function: LinkFunction,
    input_col_name: str,
    output_col_name: str,
    yhat_scale: float = None,
):
    op = MergeYAttr(link_function, input_col_name, output_col_name, yhat_scale)
    return NodeDef(name, op.name, parents=parents, data_dict=op.get_node_attr_map())


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
        return JsonModel("test_model", "just for test", graph_def).dump_tar_gz(
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
        self, path: str, config: PartyConfig, model_name: str, model_md5: str
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
                "sourceMd5": model_md5,
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
            model_name, model_md5 = self._dump_model_tar_gz(
                config_path, config.graph_def
            )
            self._dump_serving_config(config_path, config, model_name, model_md5)


# for every testcase, there should be a TestConfig instance
class TestConfig:
    def __init__(
        self,
        model_path: str,
        party_config: List[PartyConfig],
        header_dict: Dict[str, str] = None,
        service_spec_id: str = None,
        predefined_features: Dict[str, List[Any]] = None,
        log_config_name=None,
        serving_config_name=None,
        tar_name=None,
    ):
        self.header_dict = header_dict
        self.service_spec_id = service_spec_id
        self.predefined_features = predefined_features
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

    def get_module_path(self):
        return self.model_path

    def get_request_loactions(self):
        return [
            f"http://{config.cluster_ip}/PredictionService/Predict"
            for config in self.party_config
        ]

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
                pre_features.append(make_feature(name, data_list))
        else:
            pre_features = None

        if self.party_config[0].query_datas:
            fs_param = {}
            for config in self.party_config:
                fs_param[config.id] = FeatureParam(
                    config.query_datas, config.query_context
                )
        else:
            fs_param = None

        return PredictRequest(
            self.header_dict, self.service_spec_id, fs_param, pre_features
        )

    def make_curl_cmd(self, party: str):
        url = None
        for p_cfg in self.party_config:
            if p_cfg.id == party:
                url = f"http://{p_cfg.cluster_ip}/PredictionService/Predict"
                break
        if not url:
            raise Exception(f"{party} is not in TestConfig({config.get_party_ids()})")
        curl_wrapper = CurlWrapper(
            url=url,
            header="Content-Type: application/json",
            data=self.make_request().to_json(),
        )
        return curl_wrapper.cmd()

    def _exe_cmd(self, cmd, backgroud=False):
        print("Execute: ", cmd)
        if not backgroud:
            ret = subprocess.run(cmd, shell=True, check=True, capture_output=True)
            ret.check_returncode()
            return ret
        else:
            proc = subprocess.Popen(cmd.split(), shell=False)
            self.background_proc.append(proc)
            return proc

    def finish(self):
        for proc in self.background_proc:
            proc.kill()
            proc.wait()
        os.system(f"rm -rf {self.model_path}")

    def exe_start_server_scripts(self):
        main_process_name = "//secretflow_serving/server:secretflow_serving"
        self._exe_cmd(f"bazel build {main_process_name}")

        [
            self._exe_cmd(
                f"./bazel-bin/secretflow_serving/server/secretflow_serving {arg}", True
            )
            for arg in self.get_server_start_args()
        ]

    def exe_curl_request_scripts(self, party: str):
        return self._exe_cmd(self.make_curl_cmd(party))


class FeatureParam:
    def __init__(self, query_datas: List[str], query_context: str = None):
        self.query_datas = query_datas
        self.query_context = query_context

    def to_json(self):
        ret = {"query_datas": self.query_datas}
        if self.query_context:
            ret["query_context"] = self.query_context
        return ret


class FieldType(Enum):
    FIELD_BOOL = 1
    FIELD_INT32 = 2
    FIELD_INT64 = 3
    FIELD_FLOAT = 4
    FIELD_DOUBLE = 5
    FIELD_STRING = 6


class FeatureValue:
    def __init__(self, data_list, types):
        self.data_list = data_list
        assert types in ["i32s", "i64s", "fs", "ds", "ss", "bs"]
        self.types = types

    def to_json(self):
        if self.types:
            return {self.types: self.data_list}
        assert len(self.data_list) != 0
        if isinstance(self.data_list[0], int):
            types = "i32s"
        elif isinstance(self.data_list[0], float):
            types = "ds"
        elif isinstance(self.data_list[0], bool):
            types = "bs"
        else:
            types = "ss"
        return {types: self.data_list}


def make_feature(name: str, value: List[Any], f_type: FieldType = None):
    type_dict = {
        FieldType.FIELD_BOOL: "bs",
        FieldType.FIELD_DOUBLE: "ds",
        FieldType.FIELD_FLOAT: "fs",
        FieldType.FIELD_INT32: "i32s",
        FieldType.FIELD_INT64: "i64s",
        FieldType.FIELD_STRING: "ss",
    }
    if f_type:
        if f_type == FieldType.FIELD_BOOL:
            value = [bool(v) for v in value]
        elif f_type in (FieldType.FIELD_DOUBLE, FieldType.FIELD_FLOAT):
            value = [float(v) for v in value]
        elif f_type in (FieldType.FIELD_INT32, FieldType.FIELD_INT64):
            value = [int(v) for v in value]
        else:
            value = [str(v) for v in value]
        return Feature(name, f_type, FeatureValue(value, type_dict[f_type]))
    else:
        assert len(value) != 0
        if isinstance(value[0], int):
            f_type = FieldType.FIELD_INT64
        elif isinstance(value[0], float):
            f_type = FieldType.FIELD_DOUBLE
        elif isinstance(value[0], bool):
            f_type = FieldType.FIELD_BOOL
        else:
            f_type = FieldType.FIELD_STRING
            value = [str(v) for v in value]
        return Feature(name, f_type, FeatureValue(value, type_dict[f_type]))


class Feature:
    def __init__(self, field_name: str, field_type: FieldType, value: FeatureValue):
        self.name = field_name
        self.type = field_type
        self.value = value

    def to_json(self):
        return {
            "field": {"name": self.name, "type": self.type.name},
            "value": self.value.to_json(),
        }


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
                k: v.to_json() for k, v in self.party_param_dict.items()
            }
        if self.predefined_feature:
            ret["predefined_features"] = [i.to_json() for i in self.predefined_feature]
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


# simple test
def get_example_config(path: str):
    dot_node_1 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x21": -0.3, "x22": 0.95, "x23": 1.01, "x24": 1.35, "x25": -0.97},
        output_col_name="y",
        intercept=1.313,
    )
    dot_node_2 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x6": -0.53, "x7": 0.92, "x8": -0.72, "x9": 0.146, "x10": -0.07},
        output_col_name="y",
    )
    merge_y_node = make_merge_y_node_def(
        "node_merge_y",
        ["node_dot_product"],
        LinkFunction.LF_LOGIT_V2,
        input_col_name="y",
        output_col_name="score",
        yhat_scale=1.2,
    )
    execution_1 = ExecutionDef(
        ["node_dot_product"], config=RuntimeConfig(DispatchType.DP_ALL, False)
    )
    execution_2 = ExecutionDef(
        ["node_merge_y"], config=RuntimeConfig(DispatchType.DP_ANYONE, False)
    )

    alice_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_1, merge_y_node],
        execution_list=[execution_1, execution_2],
    )
    bob_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_2, merge_y_node],
        execution_list=[execution_1, execution_2],
    )

    alice_config = PartyConfig(
        id="alice",
        feature_mapping={
            "v24": "x24",
            "v22": "x22",
            "v21": "x21",
            "v25": "x25",
            "v23": "x23",
        },
        cluster_ip="127.0.0.1:9010",
        metrics_port=10306,
        brpc_builtin_service_port=10307,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=alice_graph,
        query_datas=["a"],
    )
    bob_config = PartyConfig(
        id="bob",
        feature_mapping={"v6": "x6", "v7": "x7", "v8": "x8", "v9": "x9", "v10": "x10"},
        cluster_ip="127.0.0.1:9011",
        metrics_port=10308,
        brpc_builtin_service_port=10309,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=bob_graph,
        query_datas=["b"],
    )
    return TestConfig(
        path,
        service_spec_id="integration_test",
        party_config=[alice_config, bob_config],
    )


def simple_mock_test():
    config = get_example_config("model_path")
    try:
        config.dump_config()
        config.exe_start_server_scripts()
        import time

        time.sleep(1)
        for party in config.get_party_ids():
            res = config.exe_curl_request_scripts(party)
            out = res.stdout.decode()
            print("Result: ", out)
            res = json.loads(out)
            assert res["status"]["code"] == 1, "return status code is not OK(1)"
            assert len(res["results"]) == len(
                config.party_config[0].query_datas
            ), f"result rows({len(res['results'])}) not equal to query_data({len(config.party_config[0].query_datas)})"
    finally:
        config.finish()


# predefine_test
def get_predefine_config(path: str):
    dot_node_1 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x1": 1.0, "x2": 2.0},
        output_col_name="y",
        intercept=0,
    )
    dot_node_2 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x1": -1.0, "x2": -2.0},
        output_col_name="y",
        intercept=0,
    )
    merge_y_node = make_merge_y_node_def(
        "node_merge_y",
        ["node_dot_product"],
        LinkFunction.LF_INDENTITY,
        input_col_name="y",
        output_col_name="score",
        yhat_scale=1.0,
    )
    execution_1 = ExecutionDef(
        ["node_dot_product"], config=RuntimeConfig(DispatchType.DP_ALL, False)
    )
    execution_2 = ExecutionDef(
        ["node_merge_y"], config=RuntimeConfig(DispatchType.DP_ANYONE, False)
    )

    alice_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_1, merge_y_node],
        execution_list=[execution_1, execution_2],
    )
    bob_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_2, merge_y_node],
        execution_list=[execution_1, execution_2],
    )

    alice_config = PartyConfig(
        id="alice",
        feature_mapping={},
        cluster_ip="127.0.0.1:9010",
        metrics_port=10306,
        brpc_builtin_service_port=10307,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=alice_graph,
        query_datas=["a", "a", "a"],  # only length matters
    )
    bob_config = PartyConfig(
        id="bob",
        feature_mapping={},
        cluster_ip="127.0.0.1:9011",
        metrics_port=10308,
        brpc_builtin_service_port=10309,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=bob_graph,
        query_datas=["a", "a", "a"],  # only length matters
    )
    return TestConfig(
        path,
        service_spec_id="integration_test",
        party_config=[alice_config, bob_config],
        predefined_features={
            "x1": [1.0, 2.0, 3.4],
            "x2": [6.0, 7.0, 8.0],
        },
    )


def predefine_test():
    config = get_predefine_config("module_path")
    try:
        config.dump_config()
        config.exe_start_server_scripts()
        import time

        time.sleep(1)
        results = []
        for party in config.get_party_ids():
            res = config.exe_curl_request_scripts(party)
            out = res.stdout.decode()
            print("Result: ", out)
            res = json.loads(out)
            assert (
                res["status"]["code"] == 1
            ), f'return status code({res["status"]["code"]}) is not OK(1)'
            assert len(res["results"]) == len(
                config.party_config[0].query_datas
            ), f"result rows({len(res['results'])}) not equal to query_data({len(config.party_config[0].query_datas)})"
            results.append(res)
        # std::rand in MockAdapater start with same seed at both sides
        for a_score, b_score in zip(results[0]["results"], results[1]["results"]):
            assert a_score["scores"][0]["value"] + b_score["scores"][0]["value"] == 0
    finally:
        config.finish()


# csv test
def get_csv_config(path: str):
    dot_node_1 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x1": 1.0, "x2": 2.0},
        output_col_name="y",
        intercept=0,
    )
    dot_node_2 = make_dot_product_node_def(
        name="node_dot_product",
        parents=[],
        weight_dict={"x1": -1.0, "x2": -2.0},
        output_col_name="y",
        intercept=0,
    )
    merge_y_node = make_merge_y_node_def(
        "node_merge_y",
        ["node_dot_product"],
        LinkFunction.LF_INDENTITY,
        input_col_name="y",
        output_col_name="score",
        yhat_scale=1.0,
    )
    execution_1 = ExecutionDef(
        ["node_dot_product"], config=RuntimeConfig(DispatchType.DP_ALL, False)
    )
    execution_2 = ExecutionDef(
        ["node_merge_y"], config=RuntimeConfig(DispatchType.DP_ANYONE, False)
    )

    alice_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_1, merge_y_node],
        execution_list=[execution_1, execution_2],
    )
    bob_graph = GraphDef(
        "0.0.1",
        node_list=[dot_node_2, merge_y_node],
        execution_list=[execution_1, execution_2],
    )

    alice_config = PartyConfig(
        id="alice",
        feature_mapping={"v1": "x1", "v2": "x2"},
        cluster_ip="127.0.0.1:9010",
        metrics_port=10306,
        brpc_builtin_service_port=10307,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=alice_graph,
        query_datas=["a", "b", "c"],  # Corresponds to the id column in csv
        csv_dict={
            "id": ["a", "b", "c", "d"],
            "v1": [1.0, 2.0, 3.0, 4.0],
            "v2": [5.0, 6.0, 7.0, 8.0],
        },
    )
    bob_config = PartyConfig(
        id="bob",
        feature_mapping={"vv2": "x2", "vv3": "x1"},
        cluster_ip="127.0.0.1:9011",
        metrics_port=10308,
        brpc_builtin_service_port=10309,
        channel_protocol="baidu_std",
        model_id="integration_model",
        graph_def=bob_graph,
        query_datas=["a", "b", "c"],  # Corresponds to the id column in csv
        csv_dict={
            "id": ["a", "b", "c"],
            "vv3": [1.0, 2.0, 3.0],
            "vv2": [5.0, 6.0, 7.0],
        },
    )
    return TestConfig(
        path,
        service_spec_id="integration_test",
        party_config=[alice_config, bob_config],
    )


def csv_test():
    config = get_csv_config("module_path")
    try:
        config.dump_config()
        config.exe_start_server_scripts()
        import time

        time.sleep(1)
        results = []
        for party in config.get_party_ids():
            res = config.exe_curl_request_scripts(party)
            out = res.stdout.decode()
            print("Result: ", out)
            res = json.loads(out)
            assert (
                res["status"]["code"] == 1
            ), f'return status code({res["status"]["code"]}) is not OK(1)'
            assert len(res["results"]) == len(
                config.party_config[0].query_datas
            ), f"result rows({len(res['results'])}) not equal to query_data({len(config.party_config[0].query_datas)})"
            for score in res["results"]:
                assert score["scores"][0]["value"] == 0, "result should be 0"
    finally:
        config.finish()


if __name__ == "__main__":
    simple_mock_test()
    predefine_test()
    csv_test()
