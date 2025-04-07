# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: secretflow_serving/protos/graph.proto
# Protobuf Python Version: 4.25.6
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()


from secretflow_serving_lib.secretflow_serving.protos import attr_pb2 as secretflow__serving_dot_protos_dot_attr__pb2


DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n%secretflow_serving/protos/graph.proto\x12\x12secretflow.serving\x1a$secretflow_serving/protos/attr.proto\"t\n\rRuntimeConfig\x12\x37\n\rdispatch_type\x18\x01 \x01(\x0e\x32 .secretflow.serving.DispatchType\x12\x13\n\x0bsession_run\x18\x02 \x01(\x08\x12\x15\n\rspecific_flag\x18\x03 \x01(\x08\"P\n\x0c\x45xecutionDef\x12\r\n\x05nodes\x18\x01 \x03(\t\x12\x31\n\x06\x63onfig\x18\x02 \x01(\x0b\x32!.secretflow.serving.RuntimeConfig\"\xdf\x01\n\x07NodeDef\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\n\n\x02op\x18\x02 \x01(\t\x12\x0f\n\x07parents\x18\x03 \x03(\t\x12@\n\x0b\x61ttr_values\x18\x04 \x03(\x0b\x32+.secretflow.serving.NodeDef.AttrValuesEntry\x12\x12\n\nop_version\x18\x05 \x01(\t\x1aS\n\x0f\x41ttrValuesEntry\x12\x0b\n\x03key\x18\x01 \x01(\t\x12/\n\x05value\x18\x02 \x01(\x0b\x32 .secretflow.serving.op.AttrValue:\x02\x38\x01\"I\n\x08NodeView\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\n\n\x02op\x18\x02 \x01(\t\x12\x0f\n\x07parents\x18\x03 \x03(\t\x12\x12\n\nop_version\x18\x05 \x01(\t\"\xc9\x01\n\x08GraphDef\x12\x0f\n\x07version\x18\x01 \x01(\t\x12.\n\tnode_list\x18\x02 \x03(\x0b\x32\x1b.secretflow.serving.NodeDef\x12\x38\n\x0e\x65xecution_list\x18\x03 \x03(\x0b\x32 .secretflow.serving.ExecutionDef\x12/\n\the_config\x18\x04 \x01(\x0b\x32\x1c.secretflow.serving.HeConfig\x12\x11\n\tparty_num\x18\n \x01(\x05\"\xc7\x01\n\tGraphView\x12\x0f\n\x07version\x18\x01 \x01(\t\x12/\n\tnode_list\x18\x02 \x03(\x0b\x32\x1c.secretflow.serving.NodeView\x12\x38\n\x0e\x65xecution_list\x18\x03 \x03(\x0b\x32 .secretflow.serving.ExecutionDef\x12+\n\x07he_info\x18\x04 \x01(\x0b\x32\x1a.secretflow.serving.HeInfo\x12\x11\n\tparty_num\x18\n \x01(\x05\"@\n\x08HeConfig\x12\x0e\n\x06pk_buf\x18\x01 \x01(\x0c\x12\x0e\n\x06sk_buf\x18\x02 \x01(\x0c\x12\x14\n\x0c\x65ncode_scale\x18\x03 \x01(\x03\".\n\x06HeInfo\x12\x0e\n\x06pk_buf\x18\x01 \x01(\x0c\x12\x14\n\x0c\x65ncode_scale\x18\x03 \x01(\x03*j\n\x0c\x44ispatchType\x12\x13\n\x0fUNKNOWN_DP_TYPE\x10\x00\x12\n\n\x06\x44P_ALL\x10\x01\x12\r\n\tDP_ANYONE\x10\x02\x12\x10\n\x0c\x44P_SPECIFIED\x10\x03\x12\x0b\n\x07\x44P_SELF\x10\x04\x12\x0b\n\x07\x44P_PEER\x10\x0c\x62\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'secretflow_serving.protos.graph_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:
  DESCRIPTOR._options = None
  _globals['_NODEDEF_ATTRVALUESENTRY']._options = None
  _globals['_NODEDEF_ATTRVALUESENTRY']._serialized_options = b'8\001'
  _globals['_DISPATCHTYPE']._serialized_start=1120
  _globals['_DISPATCHTYPE']._serialized_end=1226
  _globals['_RUNTIMECONFIG']._serialized_start=99
  _globals['_RUNTIMECONFIG']._serialized_end=215
  _globals['_EXECUTIONDEF']._serialized_start=217
  _globals['_EXECUTIONDEF']._serialized_end=297
  _globals['_NODEDEF']._serialized_start=300
  _globals['_NODEDEF']._serialized_end=523
  _globals['_NODEDEF_ATTRVALUESENTRY']._serialized_start=440
  _globals['_NODEDEF_ATTRVALUESENTRY']._serialized_end=523
  _globals['_NODEVIEW']._serialized_start=525
  _globals['_NODEVIEW']._serialized_end=598
  _globals['_GRAPHDEF']._serialized_start=601
  _globals['_GRAPHDEF']._serialized_end=802
  _globals['_GRAPHVIEW']._serialized_start=805
  _globals['_GRAPHVIEW']._serialized_end=1004
  _globals['_HECONFIG']._serialized_start=1006
  _globals['_HECONFIG']._serialized_end=1070
  _globals['_HEINFO']._serialized_start=1072
  _globals['_HEINFO']._serialized_end=1118
# @@protoc_insertion_point(module_scope)
