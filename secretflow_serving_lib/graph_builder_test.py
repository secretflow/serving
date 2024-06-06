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


from google.protobuf.json_format import MessageToJson

from secretflow_serving_lib import GraphBuilder, build_serving_tar

import tarfile
import unittest


class GraphBuilderTest(unittest.TestCase):
    def test_work(self):
        builder = GraphBuilder()
        attrs = {
            'feature_names': ['x1', 'x2'],
            'feature_weights': [0.1, 0.2],
            'input_types': ["DT_FLOAT", "DT_FLOAT"],
            'output_col_name': 'x',
        }
        builder.add_node(
            name='dot_product_1', op='DOT_PRODUCT', parents_name=[], **attrs
        )

        builder.begin_new_execution(dispatch_type="DP_ANYONE")

        attrs = {
            'link_function': 'LF_SIGMOID_SEG3',
            'input_col_name': 'x',
            'output_col_name': 'score',
        }
        builder.add_node(
            name='merge_y_1', op='merge_y', parents_name=['dot_product_1'], **attrs
        )

        graph = builder.build_proto()
        self.assertEqual(len(graph.node_list), 2)
        self.assertEqual(len(graph.execution_list), 2)
        self.assertEqual(graph.node_list[0].op, 'DOT_PRODUCT')
        self.assertEqual(graph.node_list[1].op, 'MERGE_Y')

        io_handle = build_serving_tar('test', 'test_model', graph)
        io_handle.seek(0)
        with tarfile.open(fileobj=io_handle, mode="r:gz") as tar:
            members = tar.getmembers()
            self.assertEqual(len(members), 2)
            self.assertEqual(members[0].name, "MANIFEST")
            self.assertEqual(members[1].name, "model_file")


if __name__ == '__main__':
    unittest.main()
