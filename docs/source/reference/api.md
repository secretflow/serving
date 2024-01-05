# SecretFlow-Serving API

## Table of Contents
- Services









    - [ExecutionService](#executionservice)





    - [metrics](#metrics)





    - [ModelService](#modelservice)





    - [PredictionService](#predictionservice)










- Messages



    - [Header](#header)
    - [Header.DataEntry](#header-dataentry)
    - [ServiceSpec](#servicespec)








    - [ExecuteRequest](#executerequest)
    - [ExecuteResponse](#executeresponse)
    - [ExecuteResult](#executeresult)
    - [ExecutionTask](#executiontask)
    - [FeatureSource](#featuresource)
    - [IoData](#iodata)
    - [NodeIo](#nodeio)





    - [MetricsRequest](#metricsrequest)
    - [MetricsResponse](#metricsresponse)





    - [GetModelInfoRequest](#getmodelinforequest)
    - [GetModelInfoResponse](#getmodelinforesponse)





    - [PredictRequest](#predictrequest)
    - [PredictRequest.FsParamsEntry](#predictrequest-fsparamsentry)
    - [PredictResponse](#predictresponse)
    - [PredictResult](#predictresult)
    - [Score](#score)





    - [Status](#status)





    - [Feature](#feature)
    - [FeatureField](#featurefield)
    - [FeatureParam](#featureparam)
    - [FeatureValue](#featurevalue)




- Enums






    - [ErrorCode](#errorcode)





    - [FeatureSourceType](#featuresourcetype)

















    - [FieldType](#fieldtype)



- [Scalar Value Types](#scalar-value-types)


 <!-- end services -->

 <!-- end services -->

{#ExecutionService}
## ExecutionService
ExecutionService provides access to run execution defined in the GraphDef.

### Execute

> **rpc** Execute([ExecuteRequest](#executerequest))
    [ExecuteResponse](#executeresponse)


 <!-- end methods -->
 <!-- end services -->

{#metrics}
## metrics


### default_method

> **rpc** default_method([MetricsRequest](#metricsrequest))
    [MetricsResponse](#metricsresponse)


 <!-- end methods -->
 <!-- end services -->

{#ModelService}
## ModelService
ModelService provides operation ralated to models.

### GetModelInfo

> **rpc** GetModelInfo([GetModelInfoRequest](#getmodelinforequest))
    [GetModelInfoResponse](#getmodelinforesponse)


 <!-- end methods -->
 <!-- end services -->

{#PredictionService}
## PredictionService
PredictionService provides access to the serving model.

### Predict

> **rpc** Predict([PredictRequest](#predictrequest))
    [PredictResponse](#predictresponse)

Predict.
 <!-- end methods -->
 <!-- end services -->

 <!-- end services -->

 <!-- end services -->
 <!-- end Files -->

## Messages



{#Header}
### Header
Header containing custom data


| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [map Header.DataEntry](#header-dataentry ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Header.DataEntry}
### Header.DataEntry



| Field | Type | Description |
| ----- | ---- | ----------- |
| key | [ string](#string ) | none |
| value | [ string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ServiceSpec}
### ServiceSpec
Metadata for an predict or execute request.


| Field | Type | Description |
| ----- | ---- | ----------- |
| id | [ string](#string ) | The id of the model service. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->

 <!-- end messages -->



{#ExecuteRequest}
### ExecuteRequest
Execute request containing one or more requests.


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. The header will be passed to the downstream system which implement the feature service spi. |
| requester_id | [ string](#string ) | Represents the id of the requesting party |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
| session_id | [ string](#string ) | Represents the session of this execute. |
| feature_source | [ FeatureSource](#featuresource ) | none |
| task | [ ExecutionTask](#executiontask ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ExecuteResponse}
### ExecuteResponse
Execute response containing one or more responses.


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. Passed by the downstream system which implement the feature service spi. |
| status | [ Status](#status ) | Staus of this response. |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
| session_id | [ string](#string ) | Represents the session of this execute. |
| result | [ ExecuteResult](#executeresult ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ExecuteResult}
### ExecuteResult
Execute result of the request task.


| Field | Type | Description |
| ----- | ---- | ----------- |
| execution_id | [ int32](#int32 ) | Specified the execution id. |
| nodes | [repeated NodeIo](#nodeio ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#ExecutionTask}
### ExecutionTask
Execute request task.


| Field | Type | Description |
| ----- | ---- | ----------- |
| execution_id | [ int32](#int32 ) | Specified the execution id. |
| nodes | [repeated NodeIo](#nodeio ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FeatureSource}
### FeatureSource
Descriptive feature source


| Field | Type | Description |
| ----- | ---- | ----------- |
| type | [ FeatureSourceType](#featuresourcetype ) | Identifies the source type of the features |
| fs_param | [ secretflow.serving.FeatureParam](#featureparam ) | Custom parameter for fetch features from feature service or other systems. Valid when `type==FeatureSourceType::FS_SERVICE` |
| predefineds | [repeated secretflow.serving.Feature](#feature ) | Defined features. Valid when `type==FeatureSourceType::FS_PREDEFINED` |
 <!-- end Fields -->
 <!-- end HasFields -->


{#IoData}
### IoData
The serialized data of the node input/output.


| Field | Type | Description |
| ----- | ---- | ----------- |
| datas | [repeated bytes](#bytes ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#NodeIo}
### NodeIo
Represents the node input/output data.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Node name. |
| ios | [repeated IoData](#iodata ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#MetricsRequest}
### MetricsRequest


 <!-- end HasFields -->


{#MetricsResponse}
### MetricsResponse


 <!-- end HasFields -->
 <!-- end messages -->



{#GetModelInfoRequest}
### GetModelInfoRequest



| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#GetModelInfoResponse}
### GetModelInfoResponse



| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. |
| status | [ Status](#status ) | Staus of this response. |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
| model_info | [ secretflow.serving.ModelInfo](#modelinfo ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#PredictRequest}
### PredictRequest
Predict request containing one or more requests.
examples:
```json
  {
    "header": {
      "data": {
        "custom_str": "id_12345"
      },
    },
    "service_spec": {
      "id": "test_service_id"
    },
    "fs_params": {
      "alice": {
        "query_datas": [
          "x1",
          "x2"
        ],
        "query_context": "context_x"
      },
      "bob": {
        "query_datas": [
          "y1",
          "y2"
        ],
        "query_context": "context_y"
      }
    }
  }
```


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. The header will be passed to the downstream system which implement the feature service spi. |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
| fs_params | [map PredictRequest.FsParamsEntry](#predictrequest-fsparamsentry ) | The params for fetch features. Note that this should include all the parties involved in the prediction. Key: party's id. Value: params for fetch features. |
| predefined_features | [repeated secretflow.serving.Feature](#feature ) | Optional. If defined, the request party will no longer query for the feature but will use defined fetures in `predefined_features` for the prediction. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#PredictRequest.FsParamsEntry}
### PredictRequest.FsParamsEntry



| Field | Type | Description |
| ----- | ---- | ----------- |
| key | [ string](#string ) | none |
| value | [ secretflow.serving.FeatureParam](#featureparam ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#PredictResponse}
### PredictResponse
Predict response containing one or more responses.
examples:
```json
  {
    "header": {
      "data": {
        "custom_value": "asdfvb"
      },
    },
    "status": {
      "code": 0,
      "msg": "success."
    },
    "service_spec": {
      "id": "test_service_id"
    },
    "results": {
      "scores": [
        {
          "name": "pred_y",
          "value": 0.32456
        },
        {
          "name": "pred_y",
          "value": 0.02456
        }
      ]
    }
  }
```


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. Passed by the downstream system which implement the feature service spi. |
| status | [ Status](#status ) | Staus of this response. |
| service_spec | [ ServiceSpec](#servicespec ) | Model service specification. |
| results | [repeated PredictResult](#predictresult ) | List of the predict result. Returned in the same order as the request's feature query data. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#PredictResult}
### PredictResult
Result of single predict request.


| Field | Type | Description |
| ----- | ---- | ----------- |
| scores | [repeated Score](#score ) | According to the model, there may be one or multi scores. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#Score}
### Score
Result of regression or one class of Classifications


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | The name of the score, it depends on the attribute configuration of the model. |
| value | [ double](#double ) | The value of the score. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#Status}
### Status
Represents the status of a request


| Field | Type | Description |
| ----- | ---- | ----------- |
| code | [ int32](#int32 ) | The code of this status. Must be one of ErrorCode in error_code.proto |
| msg | [ string](#string ) | The msg of this status. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->



{#Feature}
### Feature
The definition of a feature


| Field | Type | Description |
| ----- | ---- | ----------- |
| field | [ FeatureField](#featurefield ) | none |
| value | [ FeatureValue](#featurevalue ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FeatureField}
### FeatureField
The definition of a feature field.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Unique name of the feature |
| type | [ FieldType](#fieldtype ) | Field type of the feature |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FeatureParam}
### FeatureParam
The param for fetch features


| Field | Type | Description |
| ----- | ---- | ----------- |
| query_datas | [repeated string](#string ) | The serialized datas for query features. Each one for query one row of features. |
| query_context | [ string](#string ) | Optional. Represents the common part of the query datas. |
 <!-- end Fields -->
 <!-- end HasFields -->


{#FeatureValue}
### FeatureValue
The value of a feature


| Field | Type | Description |
| ----- | ---- | ----------- |
| i32s | [repeated int32](#int32 ) | int list |
| i64s | [repeated int64](#int64 ) | none |
| fs | [repeated float](#float ) | float list |
| ds | [repeated double](#double ) | none |
| ss | [repeated string](#string ) | string list |
| bs | [repeated bool](#bool ) | bool list |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->
 <!-- end Files -->

## Enums

 <!-- end Enums -->



### ErrorCode


| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN | 0 | Placeholder for proto3 default value, do not use it |
| OK | 1 | none |
| UNEXPECTED_ERROR | 2 | none |
| INVALID_ARGUMENT | 3 | none |
| NETWORK_ERROR | 4 | none |
| NOT_FOUND | 5 | Some requested entity (e.g., file or directory) was not found. |
| NOT_IMPLEMENTED | 6 | none |
| LOGIC_ERROR | 7 | none |
| SERIALIZE_FAILED | 8 | none |
| DESERIALIZE_FAILED | 9 | none |
| IO_ERROR | 10 | none |
| NOT_READY | 11 | none |
| FS_UNAUTHENTICATED | 100 | none |
| FS_INVALID_ARGUMENT | 101 | none |
| FS_DEADLINE_EXCEEDED | 102 | none |
| FS_NOT_FOUND | 103 | none |
| FS_INTERNAL_ERROR | 104 | none |


 <!-- end Enums -->



### FeatureSourceType
Support feature source type

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_FS_TYPE | 0 | none |
| FS_NONE | 1 | No need features. |
| FS_SERVICE | 2 | Fetch features from feature service. |
| FS_PREDEFINED | 3 | The feature is defined in the request. |


 <!-- end Enums -->

 <!-- end Enums -->

 <!-- end Enums -->

 <!-- end Enums -->

 <!-- end Enums -->



### FieldType
Supported feature field type.

| Name | Number | Description |
| ---- | ------ | ----------- |
| UNKNOWN_FIELD_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| FIELD_BOOL | 1 | BOOL |
| FIELD_INT32 | 2 | INT32 |
| FIELD_INT64 | 3 | INT64 |
| FIELD_FLOAT | 4 | FLOAT |
| FIELD_DOUBLE | 5 | DOUBLE |
| FIELD_STRING | 6 | STRING |


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
