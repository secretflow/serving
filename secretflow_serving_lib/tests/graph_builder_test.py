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


from secretflow_serving_lib import (
    GraphBuilder,
    build_serving_tar,
    bundle_pb2,
    graph_pb2,
)

import tarfile
import unittest

import json


class GraphBuilderTest(unittest.TestCase):
    def make_builder(self):
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
        return builder

        graph = builder.build_proto()

    def test_work_json(self):
        builder = self.make_builder()

        graph = builder.build_proto()
        self.assertEqual(len(graph.node_list), 2)
        self.assertEqual(len(graph.execution_list), 2)
        self.assertEqual(graph.node_list[0].op, 'DOT_PRODUCT')
        self.assertEqual(graph.node_list[1].op, 'MERGE_Y')

    def test_tar_json(self):
        graph = self.make_builder().build_proto()
        io_handle = build_serving_tar('test', 'test_model', graph)
        io_handle.seek(0)
        with tarfile.open(fileobj=io_handle, mode="r:gz") as tar:
            members = tar.getmembers()
            self.assertEqual(len(members), 2)
            self.assertEqual(members[0].name, "MANIFEST")
            manifest = tar.extractfile(members[0])
            manifest = json.loads(manifest.read())
            self.assertEqual(manifest['bundle_path'], 'model_file')
            self.assertEqual(manifest['bundle_format'], 'FF_JSON')
            self.assertEqual(members[1].name, "model_file")
            model = json.loads(tar.extractfile(members[1]).read())
            self.assertEqual(model['name'], "test")
            self.assertEqual(model['desc'], "test_model")
            self.assertEqual(len(model['graph']["node_list"]), 2)
            self.assertEqual(model['graph']["node_list"][0]['op'], 'DOT_PRODUCT')
            self.assertEqual(
                model['graph']["execution_list"][0]['config']['dispatch_type'],
                'DP_ALL',
            )

    def test_tar_pb(self):
        graph = self.make_builder().build_proto()
        io_handle = build_serving_tar('test', 'test_model', graph, bundle_type='pb')
        io_handle.seek(0)
        with tarfile.open(fileobj=io_handle, mode="r:gz") as tar:
            members = tar.getmembers()
            self.assertEqual(len(members), 2)
            self.assertEqual(members[0].name, "MANIFEST")
            manifest = tar.extractfile(members[0])
            manifest = json.loads(manifest.read())
            self.assertEqual(manifest['bundle_path'], 'model_file')
            self.assertEqual(manifest['bundle_format'], 'FF_PB')
            self.assertEqual(members[1].name, "model_file")
            model = bundle_pb2.ModelBundle.FromString(
                tar.extractfile(members[1]).read()
            )
            self.assertEqual(model.name, "test")
            self.assertEqual(model.desc, "test_model")
            self.assertEqual(len(model.graph.node_list), 2)
            self.assertEqual(model.graph.node_list[0].op, 'DOT_PRODUCT')
            self.assertEqual(
                model.graph.execution_list[0].config.dispatch_type,
                graph_pb2.DispatchType.DP_ALL,
            )


if __name__ == '__main__':
    unittest.main()
