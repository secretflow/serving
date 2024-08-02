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

from . import op_pb2
from . import libserving  # type: ignore


def get_all_ops() -> List[op_pb2.OpDef]:
    ret = []
    contents = libserving.get_all_op_defs_impl()
    for c in contents:
        o = op_pb2.OpDef()
        o.ParseFromString(c)
        ret.append(o)

    return ret


def get_op(name: str) -> op_pb2.OpDef:
    content = libserving.get_op_def_impl(name)
    o = op_pb2.OpDef()
    o.ParseFromString(content)
    return o


def get_graph_version() -> str:
    return libserving.get_graph_def_version_impl()
