import os
import sys

import secretflow_serving_lib as serving

print("about to import", file=sys.stderr)
print("python is", sys.version_info)
print("pid is", os.getpid())


print("imported, about to call", file=sys.stderr)

# get ops
ops = serving.get_all_ops()
assert len(ops) == 3

# get ops
op = serving.get_op("MERGE_Y")
print(op)

# get graph version
g_v = serving.get_graph_version()
print(g_v)

print("done!", file=sys.stderr)
