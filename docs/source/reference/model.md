# SecretFlow-Serving Model

## Table of Contents
- Services























- Messages



    - [AttrDef](#attrdef)
    - [AttrValue](#attrvalue)
    - [BoolList](#boollist)
    - [BytesList](#byteslist)
    - [DoubleList](#doublelist)
    - [FloatList](#floatlist)
    - [Int32List](#int32list)
    - [Int64List](#int64list)
    - [StringList](#stringlist)





    - [IoDef](#iodef)
    - [OpDef](#opdef)
    - [OpTag](#optag)





    - [ExecutionDef](#executiondef)
    - [GraphDef](#graphdef)
    - [GraphView](#graphview)
    - [NodeDef](#nodedef)
    - [NodeDef.AttrValuesEntry](#nodedef-attrvaluesentry)
    - [NodeView](#nodeview)
    - [RuntimeConfig](#runtimeconfig)





    - [ModelBundle](#modelbundle)
    - [ModelInfo](#modelinfo)
    - [ModelManifest](#modelmanifest)








    - [ComputeTrace](#computetrace)
    - [FunctionInput](#functioninput)
    - [FunctionOutput](#functionoutput)
    - [FunctionTrace](#functiontrace)
    - [Scalar](#scalar)





    - [ComputeTrace](#computetrace)
    - [FunctionInput](#functioninput)
    - [FunctionOutput](#functionoutput)
    - [FunctionTrace](#functiontrace)
    - [Scalar](#scalar)




- Enums



    - [AttrType](#attrtype)








    - [DispatchType](#dispatchtype)





    - [FileFormatType](#fileformattype)





    - [DataType](#datatype)





    - [ExtendFunctionName](#extendfunctionname)





    - [ExtendFunctionName](#extendfunctionname)



- [Scalar Value Types](#scalar-value-types)


 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->
 <!-- end Files -->

## Messages



{#AttrDef}
### AttrDef
The definition of an attribute.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Must be unique among all attr of the operator. |
| desc | [ string](#string ) | Description of the attribute |
| type | [ AttrType](#attrtype ) | none |
| is_optional | [ bool](#bool ) | If True, when AttrValue is not provided or is_na, default_value would be used. Else, AttrValue must be provided. |
| default_value | [ AttrValue](#attrvalue ) | A reasonable default for this attribute if it's optional and the user does not supply a value. If not, the user must supply a value. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#AttrValue}
### AttrValue
The value of an attribute


| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i32 | [ int32](#int32 ) | INT |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i64 | [ int64](#int64 ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.f | [ float](#float ) | FLOAT |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.d | [ double](#double ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.s | [ string](#string ) | STRING |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.b | [ bool](#bool ) | BOOL |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.by | [ bytes](#bytes ) | BYTES |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i32s | [ Int32List](#int32list ) | INTS |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i64s | [ Int64List](#int64list ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.fs | [ FloatList](#floatlist ) | FLOATS |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ds | [ DoubleList](#doublelist ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ss | [ StringList](#stringlist ) | STRINGS |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.bs | [ BoolList](#boollist ) | BOOLS |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.bys | [ BytesList](#byteslist ) | BYTESS |
 <!-- end Fields -->
 <!-- end HasFields -->


{#BoolList}
### BoolList



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated bool](#bool ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#BytesList}
### BytesList



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated bytes](#bytes ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#DoubleList}
### DoubleList



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated double](#double ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FloatList}
### FloatList



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated float](#float ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Int32List}
### Int32List



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated int32](#int32 ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Int64List}
### Int64List



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated int64](#int64 ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#StringList}
### StringList



| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [repeated string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#IoDef}
### IoDef
Define an input/output for operator.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Must be unique among all IOs of the operator. |
| desc | [ string](#string ) | Description of the IO |
 <!-- end Fields -->
 <!-- end HasFields -->


{#OpDef}
### OpDef
The definition of a operator.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Unique name of the op |
| desc | [ string](#string ) | Description of the op |
| version | [ string](#string ) | Version of the op |
| tag | [ OpTag](#optag ) | none |
| inputs | [repeated IoDef](#iodef ) | If tag variable_inputs is true, the op should have only one `IoDef` for inputs, referring to the parameter list. |
| output | [ IoDef](#iodef ) | none |
| attrs | [repeated AttrDef](#attrdef ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#OpTag}
### OpTag
Representation operator property


| Field | Type | Description |
| ----- | ---- | ----------- |
| returnable | [ bool](#bool ) | The operator's output can be the final result |
| mergeable | [ bool](#bool ) | The operator accept the output of operators with different participants and will somehow merge them. |
| session_run | [ bool](#bool ) | The operator needs to be executed in session. |
| variable_inputs | [ bool](#bool ) | Whether this op has variable input argument. default `false`. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#ExecutionDef}
### ExecutionDef
The definition of a execution. A execution represents a subgraph within a
graph that can be scheduled for execution in a specified pattern.


| Field | Type | Description |
| ----- | ---- | ----------- |
| nodes | [repeated string](#string ) | Represents the nodes contained in this execution. Note that these node names should be findable and unique within the node definitions. One node can only exist in one execution and must exist in one. |
| config | [ RuntimeConfig](#runtimeconfig ) | The runtime config of the execution. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#GraphDef}
### GraphDef
The definition of a Graph. A graph consists of a set of nodes carrying data
and a set of executions that describes the scheduling of the graph.


| Field | Type | Description |
| ----- | ---- | ----------- |
| version | [ string](#string ) | Version of the graph |
| node_list | [repeated NodeDef](#nodedef ) | none |
| execution_list | [repeated ExecutionDef](#executiondef ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#GraphView}
### GraphView
The view of a graph is used to display the structure of the graph, containing
only structural information and excluding the data components.


| Field | Type | Description |
| ----- | ---- | ----------- |
| version | [ string](#string ) | Version of the graph |
| node_list | [repeated NodeView](#nodeview ) | none |
| execution_list | [repeated ExecutionDef](#executiondef ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#NodeDef}
### NodeDef
The definition of a node.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Must be unique among all nodes of the graph. |
| op | [ string](#string ) | The operator name. |
| parents | [repeated string](#string ) | The parent node names of the node. The order of the parent nodes should match the order of the inputs of the node. |
| attr_values | [map NodeDef.AttrValuesEntry](#nodedef-attrvaluesentry ) | The attribute values configed in the node. Note that this should include all attrs defined in the corresponding OpDef. |
| op_version | [ string](#string ) | The operator version. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#NodeDef.AttrValuesEntry}
### NodeDef.AttrValuesEntry



| Field | Type | Description |
| ----- | ---- | ----------- |
| key | [ string](#string ) | none |
| value | [ op.AttrValue](#attrvalue ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#NodeView}
### NodeView
The view of a node, which could be public to other parties


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Must be unique among all nodes of the graph. |
| op | [ string](#string ) | The operator name. |
| parents | [repeated string](#string ) | The parent node names of the node. The order of the parent nodes should match the order of the inputs of the node. |
| op_version | [ string](#string ) | The operator version. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#RuntimeConfig}
### RuntimeConfig
The runtime config of the execution.


| Field | Type | Description |
| ----- | ---- | ----------- |
| dispatch_type | [ DispatchType](#dispatchtype ) | The dispatch type of the execution. |
| session_run | [ bool](#bool ) | The execution need run in session(stateful) TODO: not support yet. |
| specific_flag | [ bool](#bool ) | if dispatch_type is DP_SPECIFIED, only one party should be true |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#ModelBundle}
### ModelBundle
Represents an exported secertflow model. It consists of a GraphDef and extra
metadata required for serving.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | none |
| desc | [ string](#string ) | none |
| graph | [ GraphDef](#graphdef ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ModelInfo}
### ModelInfo
Represents a secertflow model without private data.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | none |
| desc | [ string](#string ) | none |
| graph_view | [ GraphView](#graphview ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ModelManifest}
### ModelManifest
The manifest of the model package. Package format is as follows:
model.tar.gz
 ├ MANIFIEST
 ├ model_file
 └ some op meta files
MANIFIEST should be json format


| Field | Type | Description |
| ----- | ---- | ----------- |
| bundle_path | [ string](#string ) | Model bundle file path. |
| bundle_format | [ FileFormatType](#fileformattype ) | The format type of the model bundle file. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->

 <!-- end messages -->



{#ComputeTrace}
### ComputeTrace



| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | The name of this Compute. |
| func_traces | [repeated FunctionTrace](#functiontrace ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionInput}
### FunctionInput



| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.data_id | [ int32](#int32 ) | '0' means root input data |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.custom_scalar | [ Scalar](#scalar ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionOutput}
### FunctionOutput



| Field | Type | Description |
| ----- | ---- | ----------- |
| data_id | [ int32](#int32 ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionTrace}
### FunctionTrace



| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | The Function name. |
| option_bytes | [ bytes](#bytes ) | The serialized function options. |
| inputs | [repeated FunctionInput](#functioninput ) | Inputs of this function. |
| output | [ FunctionOutput](#functionoutput ) | Output of this function. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Scalar}
### Scalar
Represents a single value with a specific data type.


| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i8 | [ int32](#int32 ) | INT8. |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui8 | [ int32](#int32 ) | UINT8 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i16 | [ int32](#int32 ) | INT16 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui16 | [ int32](#int32 ) | UINT16 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i32 | [ int32](#int32 ) | INT32 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui32 | [ uint32](#uint32 ) | UINT32 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i64 | [ int64](#int64 ) | INT64 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui64 | [ uint64](#uint64 ) | UINT64 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.f | [ float](#float ) | FLOAT |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.d | [ double](#double ) | DOUBLE |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.s | [ string](#string ) | STRING |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.b | [ bool](#bool ) | BOOL |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#ComputeTrace}
### ComputeTrace



| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | The name of this Compute. |
| func_traces | [repeated FunctionTrace](#functiontrace ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionInput}
### FunctionInput



| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.data_id | [ int32](#int32 ) | '0' means root input data |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.custom_scalar | [ Scalar](#scalar ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionOutput}
### FunctionOutput



| Field | Type | Description |
| ----- | ---- | ----------- |
| data_id | [ int32](#int32 ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FunctionTrace}
### FunctionTrace



| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | The Function name. |
| option_bytes | [ bytes](#bytes ) | The serialized function options. |
| inputs | [repeated FunctionInput](#functioninput ) | Inputs of this function. |
| output | [ FunctionOutput](#functionoutput ) | Output of this function. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Scalar}
### Scalar
Represents a single value with a specific data type.


| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i8 | [ int32](#int32 ) | INT8. |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui8 | [ int32](#int32 ) | UINT8 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i16 | [ int32](#int32 ) | INT16 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui16 | [ int32](#int32 ) | UINT16 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i32 | [ int32](#int32 ) | INT32 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui32 | [ uint32](#uint32 ) | UINT32 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.i64 | [ int64](#int64 ) | INT64 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.ui64 | [ uint64](#uint64 ) | UINT64 |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.f | [ float](#float ) | FLOAT |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.d | [ double](#double ) | DOUBLE |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.s | [ string](#string ) | STRING |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) value.b | [ bool](#bool ) | BOOL |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->
 <!-- end Files -->

## Enums



{#AttrType}
### AttrType
Supported attribute types.

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_AT_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| AT_INT32 | 1 | INT32 |
| AT_INT64 | 2 | INT64 |
| AT_FLOAT | 3 | FLOAT |
| AT_DOUBLE | 4 | DOUBLE |
| AT_STRING | 5 | STRING |
| AT_BOOL | 6 | BOOL |
| AT_BYTES | 7 | BYTES |
| AT_INT32_LIST | 11 | INT32 LIST |
| AT_INT64_LIST | 12 | INT64 LIST |
| AT_FLOAT_LIST | 13 | FLOAT LIST |
| AT_DOUBLE_LIST | 14 | DOUBLE LIST |
| AT_STRING_LIST | 15 | STRING LIST |
| AT_BOOL_LIST | 16 | BOOL LIST |
| AT_BYTES_LIST | 17 | BYTES LIST |


 <!-- end Enums -->

 <!-- end Enums -->



{#DispatchType}
### DispatchType
Supported dispatch type

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_DP_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| DP_ALL | 1 | Dispatch all participants. |
| DP_ANYONE | 2 | Dispatch any participant. |
| DP_SPECIFIED | 3 | Dispatch specified participant. |


 <!-- end Enums -->



{#FileFormatType}
### FileFormatType
Support model file format

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_FF_TYPE | 0 | none |
| FF_PB | 1 | Protobuf |
| FF_JSON | 2 | Json It is recommended to use protobuf's official json serialization method to ensure compatibility |


 <!-- end Enums -->



{#DataType}
### DataType
Mapping arrow::DataType
`https://arrow.apache.org/docs/cpp/api/datatype.html`.

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_DT_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| DT_BOOL | 1 | Boolean as 1 bit, LSB bit-packed ordering. |
| DT_UINT8 | 2 | Unsigned 8-bit little-endian integer. |
| DT_INT8 | 3 | Signed 8-bit little-endian integer. |
| DT_UINT16 | 4 | Unsigned 16-bit little-endian integer. |
| DT_INT16 | 5 | Signed 16-bit little-endian integer. |
| DT_UINT32 | 6 | Unsigned 32-bit little-endian integer. |
| DT_INT32 | 7 | Signed 32-bit little-endian integer. |
| DT_UINT64 | 8 | Unsigned 64-bit little-endian integer. |
| DT_INT64 | 9 | Signed 64-bit little-endian integer. |
| DT_FLOAT | 11 | 4-byte floating point value |
| DT_DOUBLE | 12 | 8-byte floating point value |
| DT_STRING | 13 | UTF8 variable-length string as List<Char> |
| DT_BINARY | 14 | Variable-length bytes (no guarantee of UTF8-ness) |


 <!-- end Enums -->



{#ExtendFunctionName}
### ExtendFunctionName


| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKOWN_EX_FUNCTION_NAME | 0 | Placeholder for proto3 default value, do not use it |
| EFN_TB_COLUMN | 1 | Get colunm from table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch6columnEi |
| EFN_TB_ADD_COLUMN | 2 | Add colum to table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch9AddColumnEiNSt6stringERKNSt10shared_ptrI5ArrayEE |
| EFN_TB_REMOVE_COLUMN | 3 | Remove colunm from table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch12RemoveColumnEi |
| EFN_TB_SET_COLUMN | 4 | Set colunm to table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch9SetColumnEiRKNSt10shared_ptrI5FieldEERKNSt10shared_ptrI5ArrayEE |


 <!-- end Enums -->



{#ExtendFunctionName}
### ExtendFunctionName


| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKOWN_EX_FUNCTION_NAME | 0 | Placeholder for proto3 default value, do not use it |
| EFN_TB_COLUMN | 1 | Get colunm from table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch6columnEi |
| EFN_TB_ADD_COLUMN | 2 | Add colum to table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch9AddColumnEiNSt6stringERKNSt10shared_ptrI5ArrayEE |
| EFN_TB_REMOVE_COLUMN | 3 | Remove colunm from table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch12RemoveColumnEi |
| EFN_TB_SET_COLUMN | 4 | Set colunm to table(record_batch). see https://arrow.apache.org/docs/cpp/api/table.html#_CPPv4NK5arrow11RecordBatch9SetColumnEiRKNSt10shared_ptrI5FieldEERKNSt10shared_ptrI5ArrayEE |


 <!-- end Enums -->
 <!-- end Files -->

## Scalar Value Types

| .proto Type | Notes | C++ Type | Java Type | Python Type |
| ----------- | ----- | -------- | --------- | ----------- |
| <div><h4 id="double" /></div><a name="double" /> double |  | double | double | float |
| <div><h4 id="float" /></div><a name="float" /> float |  | float | float | float |
| <div><h4 id="int32" /></div><a name="int32" /> int32 | Uses variable-length encoding. Inefficient for encoding negative numbers – if your field is likely to have negative values, use sint32 instead. | int32 | int | int |
| <div><h4 id="int64" /></div><a name="int64" /> int64 | Uses variable-length encoding. Inefficient for encoding negative numbers – if your field is likely to have negative values, use sint64 instead. | int64 | long | int/long |
| <div><h4 id="uint32" /></div><a name="uint32" /> uint32 | Uses variable-length encoding. | uint32 | int | int/long |
| <div><h4 id="uint64" /></div><a name="uint64" /> uint64 | Uses variable-length encoding. | uint64 | long | int/long |
| <div><h4 id="sint32" /></div><a name="sint32" /> sint32 | Uses variable-length encoding. Signed int value. These more efficiently encode negative numbers than regular int32s. | int32 | int | int |
| <div><h4 id="sint64" /></div><a name="sint64" /> sint64 | Uses variable-length encoding. Signed int value. These more efficiently encode negative numbers than regular int64s. | int64 | long | int/long |
| <div><h4 id="fixed32" /></div><a name="fixed32" /> fixed32 | Always four bytes. More efficient than uint32 if values are often greater than 2^28. | uint32 | int | int |
| <div><h4 id="fixed64" /></div><a name="fixed64" /> fixed64 | Always eight bytes. More efficient than uint64 if values are often greater than 2^56. | uint64 | long | int/long |
| <div><h4 id="sfixed32" /></div><a name="sfixed32" /> sfixed32 | Always four bytes. | int32 | int | int |
| <div><h4 id="sfixed64" /></div><a name="sfixed64" /> sfixed64 | Always eight bytes. | int64 | long | int/long |
| <div><h4 id="bool" /></div><a name="bool" /> bool |  | bool | boolean | boolean |
| <div><h4 id="string" /></div><a name="string" /> string | A string must always contain UTF-8 encoded or 7-bit ASCII text. | string | String | str/unicode |
| <div><h4 id="bytes" /></div><a name="bytes" /> bytes | May contain any arbitrary sequence of bytes. | string | ByteString | str |
