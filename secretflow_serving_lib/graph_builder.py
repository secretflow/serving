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

import io
import tarfile
from itertools import chain
from typing import Any, Dict, List

import numpy as np
from google.protobuf import json_format

from . import libserving  # type: ignore
from .attr_pb2 import AttrType, AttrValue
from .bundle_pb2 import FileFormatType, ModelBundle, ModelInfo, ModelManifest
from .graph_pb2 import (
    DispatchType,
    ExecutionDef,
    GraphDef,
    GraphView,
    NodeDef,
    RuntimeConfig,
)
from .op_pb2 import OpDef

from .api import get_op


def construct_attr_value(attr_type: AttrType, value) -> AttrValue:
    ret = AttrValue()
    if attr_type == AttrType.AT_BOOL:
        ret.b = bool(value)
    elif attr_type == AttrType.AT_FLOAT:
        ret.f = np.float32(value)
    elif attr_type == AttrType.AT_DOUBLE:
        ret.d = np.float64(value)
    elif attr_type == AttrType.AT_INT32:
        ret.i32 = np.int32(value)
    elif attr_type == AttrType.AT_INT64:
        ret.i64 = np.int64(value)
    elif attr_type == AttrType.AT_STRING:
        assert isinstance(value, (np.str_, str))
        ret.s = value
    elif attr_type == AttrType.AT_BYTES:
        assert isinstance(value, (np.bytes_, bytes))
        ret.by = value
    elif attr_type == AttrType.AT_BOOL_LIST:
        ret.bs.data.extend(map(bool, value))
    elif attr_type == AttrType.AT_INT32_LIST:
        ret.i32s.data.extend(map(np.int32, value))
    elif attr_type == AttrType.AT_INT64_LIST:
        ret.i64s.data.extend(map(np.int64, value))
    elif attr_type == AttrType.AT_FLOAT_LIST:
        ret.fs.data.extend(map(np.float32, value))
    elif attr_type == AttrType.AT_DOUBLE_LIST:
        ret.ds.data.extend(map(np.float64, value))
    elif attr_type == AttrType.AT_STRING_LIST:
        assert isinstance(value, list) and all(
            [isinstance(p, (np.str_, str)) for p in value]
        )
        ret.ss.data.extend(value)
    elif attr_type == AttrType.AT_BYTES_LIST:
        assert isinstance(value, list) and all(
            [isinstance(p, (np.bytes_, bytes)) for p in value]
        )
        ret.bys.data.extend(value)
    else:
        raise Exception(f"unsupported attr attr_type: {attr_type}")

    return ret


def get_op(op: str):
    contents = libserving.get_all_op_defs_impl()
    for c in contents:
        o = OpDef()
        o.ParseFromString(c)
        if op.lower() == o.name.lower():
            return o
    raise Exception("can't find op def : {op}")


def construct_attr_dict(op: str, attrs: dict) -> dict:
    op_def = get_op(op)
    attr_dict = {}
    attr_params = set(attrs.keys())
    for attr in op_def.attrs:
        if attr.name not in attrs:
            assert attr.is_optional, f"{attr.name} is not optional"
            continue
        try:
            attr_dict[attr.name] = construct_attr_value(attr.type, attrs[attr.name])
            attr_params.remove(attr.name)
        except Exception as e:
            raise AssertionError(
                f"get_op_attr err {e} on attr.name {attr.name}, "
                f"attr.type {AttrType.Name(attr.type)}, "
                f"attr {attrs[attr.name]}, attr attr_type {type(attrs[attr.name])}"
            )
    if len(attr_params) > 0:
        raise Exception(f"params {attr_params} is not needed by {op}")
    return attr_dict


class _NodeProtoWrapper:
    def __init__(self, name: str, op: str, parents_name: List[str], attrs: dict):
        self.name = name
        self.parents_name = parents_name
        self.attr_dict = construct_attr_dict(op, attrs)
        self.node = NodeDef(
            name=name,
            op=get_op(op).name,
            parents=parents_name,
            op_version=get_op(op).version,
        )
        for k, v in self.attr_dict.items():
            self.node.attr_values[k].CopyFrom(v)

    def proto(self) -> NodeDef:
        return self.node


class _ExecutionProtoWrapper:
    def __init__(
        self,
        dispatch_type: DispatchType = DispatchType.DP_ALL,
        specific_flag: bool = False,
        session_run: bool = False,
    ):
        self.runtime_config = RuntimeConfig(
            dispatch_type=dispatch_type,
            specific_flag=specific_flag,
            session_run=session_run,
        )
        self.node = []

    def add_node(self, node: _NodeProtoWrapper):
        self.node.append(node)

    def get_nodes(self) -> List[NodeDef]:
        return self.node

    def proto(self) -> ExecutionDef:
        return ExecutionDef(
            config=self.runtime_config, nodes=[node.name for node in self.node]
        )


class _GraphProtoWrapper:
    def __init__(self, version: str):
        self.version = version
        self.executions = []

    def add_execution(self, exec: _ExecutionProtoWrapper):
        self.executions.append(exec)

    def get_execution(self, idx: int) -> _ExecutionProtoWrapper:
        return self.executions[idx]

    def get_execution_count(self):
        return len(self.executions)

    def proto(self) -> GraphDef:
        return GraphDef(
            version=self.version,
            execution_list=[exec.proto() for exec in self.executions],
            node_list=list(
                chain.from_iterable(
                    [node.proto() for node in exec.get_nodes()]
                    for exec in self.executions
                )
            ),
        )


class GraphBuilder:
    def __init__(self):
        self.nodes = set()
        self.graph = _GraphProtoWrapper(libserving.get_graph_def_version_impl())

        # add a dispathch all execution to graph
        self.begin_new_execution()

    def add_node(self, name: str, op: str, parents_name: List[str], **kwargs):
        assert self.graph.get_execution_count() > 0, "no execution added yet"
        if name in self.nodes:
            raise Exception(f"node {name} already exists")
        self.nodes.add(name)
        self.graph.get_execution(-1).add_node(
            _NodeProtoWrapper(name, op, parents_name, kwargs)
        )

    def get_execution_count(self):
        return len(self.graph.executions)

    def begin_new_execution(
        self,
        dispatch_type: str = "DP_ALL",
        specific_flag: bool = False,
        session_run: bool = False,
    ):
        return self.graph.add_execution(
            _ExecutionProtoWrapper(
                DispatchType.Value(dispatch_type), specific_flag, session_run
            )
        )

    def build_proto(self) -> GraphDef:
        graph_def_str = libserving.graph_validator_impl(
            self.graph.proto().SerializeToString()
        )
        graph = GraphDef()
        graph.ParseFromString(graph_def_str)
        return graph

    def build_view_proto(self) -> GraphView:
        graph_view_str = libserving.get_graph_view_impl(
            self.graph.proto().SerializeToString()
        )
        graph_view = GraphView()
        graph_view.ParseFromString(graph_view_str)
        return graph_view


def build_serving_tar(
    name: str, desc: str, graph_def: GraphDef, bundle_type: str = "json"
) -> io.BytesIO:
    """
    bundle_type: json or pb
    """
    assert bundle_type in [
        "json",
        "pb",
    ], f"bundle_type:{bundle_type} should be json or pb"
    bundle = ModelBundle()
    bundle.name = name
    bundle.desc = desc
    bundle.graph.CopyFrom(graph_def)

    meta = ModelManifest()
    meta.bundle_path = "model_file"
    meta_data = json_format.MessageToJson(meta, indent=0).encode("utf-8")

    if bundle_type == "json":
        meta.bundle_format = FileFormatType.FF_JSON
        bundle_data = json_format.MessageToJson(bundle, indent=0).encode("utf-8")
    elif bundle_type == "pb":
        meta.bundle_format = FileFormatType.FF_PB
        bundle_data = bundle.SerializeToString()

    io_handle = io.BytesIO()
    with tarfile.open(fileobj=io_handle, mode="w:gz") as tar:
        info = tarfile.TarInfo("MANIFEST")
        info.size = len(meta_data)
        info.mode = int("0666", base=8)
        tar.addfile(info, io.BytesIO(initial_bytes=meta_data))

        info = tarfile.TarInfo("model_file")
        info.size = len(bundle_data)
        info.mode = int("0666", base=8)
        tar.addfile(info, io.BytesIO(initial_bytes=bundle_data))
    return io_handle


def check_graph_views(graph_view_dict: Dict[str, str]):
    libserving.check_graph_view_impl(graph_view_dict)
