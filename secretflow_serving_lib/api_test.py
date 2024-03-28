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


import os
import sys

import secretflow_serving_lib as serving

print("about to import", file=sys.stderr)
print("python is", sys.version_info)
print("pid is", os.getpid())


print("imported, about to call", file=sys.stderr)

# get ops
ops = serving.get_all_ops()
assert len(ops) > 0

# get ops
op = serving.get_op("MERGE_Y")
print(op)

# get graph version
g_v = serving.get_graph_version()
print(g_v)

print("done!", file=sys.stderr)
