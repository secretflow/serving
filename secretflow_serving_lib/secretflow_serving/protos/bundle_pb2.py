# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: secretflow_serving/protos/bundle.proto
# Protobuf Python Version: 4.25.6
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


from secretflow_serving_lib.secretflow_serving.protos import graph_pb2 as secretflow__serving_dot_protos_dot_graph__pb2


DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n&secretflow_serving/protos/bundle.proto\x12\x12secretflow.serving\x1a%secretflow_serving/protos/graph.proto\"V\n\x0bModelBundle\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\x0c\n\x04\x64\x65sc\x18\x02 \x01(\t\x12+\n\x05graph\x18\x03 \x01(\x0b\x32\x1c.secretflow.serving.GraphDef\"_\n\rModelManifest\x12\x13\n\x0b\x62undle_path\x18\x01 \x01(\t\x12\x39\n\rbundle_format\x18\x02 \x01(\x0e\x32\".secretflow.serving.FileFormatType\"Z\n\tModelInfo\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\x0c\n\x04\x64\x65sc\x18\x02 \x01(\t\x12\x31\n\ngraph_view\x18\x03 \x01(\x0b\x32\x1d.secretflow.serving.GraphView*=\n\x0e\x46ileFormatType\x12\x13\n\x0fUNKNOWN_FF_TYPE\x10\x00\x12\t\n\x05\x46\x46_PB\x10\x01\x12\x0b\n\x07\x46\x46_JSON\x10\x02\x62\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'secretflow_serving.protos.bundle_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:
  DESCRIPTOR._options = None
  _globals['_FILEFORMATTYPE']._serialized_start=378
  _globals['_FILEFORMATTYPE']._serialized_end=439
  _globals['_MODELBUNDLE']._serialized_start=101
  _globals['_MODELBUNDLE']._serialized_end=187
  _globals['_MODELMANIFEST']._serialized_start=189
  _globals['_MODELMANIFEST']._serialized_end=284
  _globals['_MODELINFO']._serialized_start=286
  _globals['_MODELINFO']._serialized_end=376
# @@protoc_insertion_point(module_scope)
