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

from __future__ import annotations

from typing import List

from . import libserving  # type: ignore
from . import op_pb2


def get_all_ops() -> List[op_pb2.OpDef]:
    '''Get all OpDef of Operators provided by Secretflow-Serving.'''
    ret = []
    contents = libserving.get_all_op_defs_impl()
    for c in contents:
        o = op_pb2.OpDef()
        o.ParseFromString(c)
        ret.append(o)

    return ret


def get_op(name: str) -> op_pb2.OpDef:
    '''Get the OpDef of specific Operator provided by Secretflow-Serving.

    Args:
        name: The name of Operator.
    '''
    ops = get_all_ops()
    for op in ops:
        if name.lower() == op.name.lower():
            return op
    raise ValueError(f"op {name} not found in {[op.name for op in ops]}")


def get_graph_version() -> str:
    '''Get the version of GraphDef defined in Secretflow-Serving.'''
    return libserving.get_graph_def_version_impl()
