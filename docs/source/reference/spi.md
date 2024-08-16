# SecretFlow-Serving SPI

## Table of Contents
- Services



    - [BatchFeatureService](#batchfeatureservice)













- Messages



    - [BatchFetchFeatureRequest](#batchfetchfeaturerequest)
    - [BatchFetchFeatureResponse](#batchfetchfeatureresponse)





    - [Header](#header)
    - [Header.DataEntry](#header-dataentry)
    - [Status](#status)








    - [Feature](#feature)
    - [FeatureField](#featurefield)
    - [FeatureParam](#featureparam)
    - [FeatureValue](#featurevalue)




- Enums









    - [ErrorCode](#errorcode)





    - [FieldType](#fieldtype)



- [Scalar Value Types](#scalar-value-types)


## BatchFeatureService
BatchFeatureService provides access to fetch features.

### BatchFetchFeature

> **rpc** BatchFetchFeature([BatchFetchFeatureRequest](#batchfetchfeaturerequest))
    [BatchFetchFeatureResponse](#batchfetchfeatureresponse)


 <!-- end methods -->
 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->
 <!-- end Files -->

## Messages




### BatchFetchFeatureRequest
BatchFetchFeature request containing one or more requests.
examples:
```json
  {
    "header": {
      "data": {
        "custom_str": "id_12345"
      }
    },
    "modelServiceId":  "test_service_id",
    "partyId": "alice",
    "featureFields": [
      {
        "name": "f1",
        "type": 2
      },
      {
        "name": "f2",
        "type": 4
      }
    ],
    "param": {
      "queryDatas": [
        "x1",
        "x2"
      ],
      "queryContext": "context_x"
    }
  }
```


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data passed by the Predict request's header. |
| model_service_id | [ string](#string ) | Model service specification. |
| party_id | [ string](#string ) | The request party id. |
| feature_fields | [repeated secretflow.serving.FeatureField](#featurefield ) | Request feature field list |
| param | [ secretflow.serving.FeatureParam](#featureparam ) | Custom query paramters for fetch features |
 <!-- end Fields -->
 <!-- end HasFields -->



### BatchFetchFeatureResponse
BatchFetchFeatureResponse response containing one or more responses.
examples:
```json
  {
    "header": {
      "data": {
        "custom_value": "asdfvb"
      }
    },
    "status": {
      "code": 0,
      "msg": "success."
    },
    "features": [
      {
        "field": {
          "name": "f1",
          "type": 2
        },
        "value": {
          "i32s": [
            123,
            234
          ]
        }
      },
      {
        "field": {
          "name": "f2",
          "type": 4
        },
        "value": {
          "fs": [
            0.123,
            1.234
          ]
        }
      }
    ]
  }
```


| Field | Type | Description |
| ----- | ---- | ----------- |
| header | [ Header](#header ) | Custom data. |
| status | [ Status](#status ) | none |
| features | [repeated secretflow.serving.Feature](#feature ) | Should include all the features mentioned in the BatchFetchFeatureRequest.feature_fields |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### Header
Header containing custom data


| Field | Type | Description |
| ----- | ---- | ----------- |
| data | [map Header.DataEntry](#header-dataentry ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### Header.DataEntry



| Field | Type | Description |
| ----- | ---- | ----------- |
| key | [ string](#string ) | none |
| value | [ string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### Status
Represents the status of spi request


| Field | Type | Description |
| ----- | ---- | ----------- |
| code | [ int32](#int32 ) | code value reference `ErrorCode` in secretflow_serving/spis/error_code.proto |
| msg | [ string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->

 <!-- end messages -->




### Feature
The definition of a feature


| Field | Type | Description |
| ----- | ---- | ----------- |
| field | [ FeatureField](#featurefield ) | none |
| value | [ FeatureValue](#featurevalue ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### FeatureField
The definition of a feature field.


| Field | Type | Description |
| ----- | ---- | ----------- |
| name | [ string](#string ) | Unique name of the feature |
| type | [ FieldType](#fieldtype ) | Field type of the feature |
 <!-- end Fields -->
 <!-- end HasFields -->



### FeatureParam
The param for fetch features


| Field | Type | Description |
| ----- | ---- | ----------- |
| query_datas | [repeated string](#string ) | The serialized datas for query features. Each one for query one row of features. |
| query_context | [ string](#string ) | Optional. Represents the common part of the query datas. |
 <!-- end Fields -->
 <!-- end HasFields -->



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

 <!-- end Enums -->



### ErrorCode
ONLY for Reference by ResponseHeader
It's subset of google.rpc.Code

| Name | Number | Description |
| ---- | ------ | ----------- |
| OK | 0 | Not an error; returned on success

HTTP Mapping: 200 OK |
| INVALID_ARGUMENT | 3 | The client specified an invalid argument. Note that this differs from `FAILED_PRECONDITION`. `INVALID_ARGUMENT` indicates arguments that are problematic regardless of the state of the system (e.g., a malformed file name).

HTTP Mapping: 400 Bad Request |
| DEADLINE_EXCEEDED | 4 | The deadline expired before the operation could complete. For operations that change the state of the system, this error may be returned even if the operation has completed successfully. For example, a successful response from a server could have been delayed long enough for the deadline to expire.

HTTP Mapping: 504 Gateway Timeout |
| NOT_FOUND | 5 | Some requested entity (e.g., file or directory) was not found.

Note to server developers: if a request is denied for an entire class of users, such as gradual feature rollout or undocumented whitelist, `NOT_FOUND` may be used. If a request is denied for some users within a class of users, such as user-based access control, `PERMISSION_DENIED` must be used.

HTTP Mapping: 404 Not Found |
| INTERNAL_ERROR | 13 | Internal errors. This means that some invariants expected by the underlying system have been broken. This error code is reserved for serious errors.

HTTP Mapping: 500 Internal Server Error |
| UNAUTHENTICATED | 16 | The request does not have valid authentication credentials for the operation.

HTTP Mapping: 401 Unauthorized |


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

