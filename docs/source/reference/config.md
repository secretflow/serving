# SecretFlow-Serving Config

## Table of Contents
- Services





























- Messages



    - [ChannelDesc](#channeldesc)
    - [ClusterConfig](#clusterconfig)
    - [PartyDesc](#partydesc)





    - [CsvOptions](#csvoptions)
    - [FeatureSourceConfig](#featuresourceconfig)
    - [HttpOptions](#httpoptions)
    - [MockOptions](#mockoptions)
    - [StreamingOptions](#streamingoptions)





    - [LoggingConfig](#loggingconfig)





    - [DPSourceMeta](#dpsourcemeta)
    - [FileSourceMeta](#filesourcemeta)
    - [HttpSourceMeta](#httpsourcemeta)
    - [ModelConfig](#modelconfig)
    - [OSSSourceMeta](#osssourcemeta)





    - [ExponentialBackOffConfig](#exponentialbackoffconfig)
    - [FixedBackOffConfig](#fixedbackoffconfig)
    - [RandomBackOffConfig](#randombackoffconfig)
    - [RetryPolicyConfig](#retrypolicyconfig)





    - [ServerConfig](#serverconfig)
    - [ServerConfig.FeatureMappingEntry](#serverconfig-featuremappingentry)





    - [ServingConfig](#servingconfig)





    - [TlsConfig](#tlsconfig)





    - [TraceConfig](#traceconfig)
    - [TraceLogConfig](#tracelogconfig)




- Enums






    - [MockDataType](#mockdatatype)





    - [LogLevel](#loglevel)





    - [SourceType](#sourcetype)





    - [RetryPolicyBackOffMode](#retrypolicybackoffmode)















- [Scalar Value Types](#scalar-value-types)


 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->

 <!-- end services -->
 <!-- end Files -->

## Messages




### ChannelDesc
Description for channels between joined parties


| Field | Type | Description |
| ----- | ---- | ----------- |
| protocol | [ string](#string ) | https://github.com/apache/brpc/blob/master/docs/en/client.md#protocols |
| rpc_timeout_ms | [ int32](#int32 ) | Max duration of RPC. -1 means wait indefinitely. Default: 2000 (ms) |
| connect_timeout_ms | [ int32](#int32 ) | Max duration for a connect. -1 means wait indefinitely. Default: 500 (ms) |
| tls_config | [ TlsConfig](#tlsconfig ) | TLS related config. |
| handshake_max_retry_cnt | [ int32](#int32 ) | When the server starts, model information from all parties will be collected. At this time, the remote servers may not have started yet, and we need to retry. And if we connect gateway,the max waiting time for each operation will be rpc_timeout_ms + handshake_retry_interval_ms. Maximum number of retries, default: 60 |
| handshake_retry_interval_ms | [ int32](#int32 ) | time between retries, default: 5000ms |
| retry_policy_config | [ RetryPolicyConfig](#retrypolicyconfig ) | retry policy config |
 <!-- end Fields -->
 <!-- end HasFields -->



### ClusterConfig
Runtime config for a serving cluster


| Field | Type | Description |
| ----- | ---- | ----------- |
| parties | [repeated PartyDesc](#partydesc ) | none |
| self_id | [ string](#string ) | none |
| channel_desc | [ ChannelDesc](#channeldesc ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### PartyDesc
Description for a joined party


| Field | Type | Description |
| ----- | ---- | ----------- |
| id | [ string](#string ) | Unique id of the party |
| address | [ string](#string ) | e.g. 127.0.0.1:9001 commucation adress between paries. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### CsvOptions
Options of a csv feature source.


| Field | Type | Description |
| ----- | ---- | ----------- |
| file_path | [ string](#string ) | Input file path, specifies where to load data Note that this will load all of the data into memory at once |
| id_name | [ string](#string ) | Id column name, associated with FeatureParam::query_datas Query datas is a subset of id column |
| streaming_mode | [ bool](#bool ) | Optional. Only for Inferencer tool use. Defatult: false |
| block_size | [ int32](#int32 ) | Optional. Valid only if `streaming_mode=true`. This determines the size(byte) of each read batch. |
 <!-- end Fields -->
 <!-- end HasFields -->



### FeatureSourceConfig
Config for a feature source


| Field | Type | Description |
| ----- | ---- | ----------- |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) options.mock_opts | [ MockOptions](#mockoptions ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) options.http_opts | [ HttpOptions](#httpoptions ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) options.csv_opts | [ CsvOptions](#csvoptions ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) options.streaming_opts | [ StreamingOptions](#streamingoptions ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### HttpOptions
Options for a http feature source which should implement the feature service
spi. The defined of spi can be found in
secretflow_serving/spis/batch_feature_service.proto


| Field | Type | Description |
| ----- | ---- | ----------- |
| endpoint | [ string](#string ) | none |
| enable_lb | [ bool](#bool ) | Whether to enable round robin load balancer. Default: False |
| connect_timeout_ms | [ int32](#int32 ) | Max duration for a connect. -1 means wait indefinitely. Default: 500 (ms) |
| timeout_ms | [ int32](#int32 ) | Max duration of http request. -1 means wait indefinitely. Default: 1000 (ms) |
| tls_config | [ TlsConfig](#tlsconfig ) | TLS related config. |
| retry_policy_config | [ RetryPolicyConfig](#retrypolicyconfig ) | Retry policy config. |
 <!-- end Fields -->
 <!-- end HasFields -->



### MockOptions
Options for a mock feature source.
Mock feature source will generates values(random or fixed, according to type)
for the desired features.


| Field | Type | Description |
| ----- | ---- | ----------- |
| type | [ MockDataType](#mockdatatype ) | default MDT_FIXED |
 <!-- end Fields -->
 <!-- end HasFields -->



### StreamingOptions
Only for Inferencer tool use.


| Field | Type | Description |
| ----- | ---- | ----------- |
| file_path | [ string](#string ) | Input file path, specifies where to load data |
| file_format | [ string](#string ) | Input file format. Optional value: CSV Default: CSV |
| id_name | [ string](#string ) | Id column name, associated with FeatureParam::query_datas Query datas is a subset of id column |
| block_size | [ int32](#int32 ) | Optional. This determines the size(byte) of each read batch. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### LoggingConfig
Serving log config options


| Field | Type | Description |
| ----- | ---- | ----------- |
| system_log_path | [ string](#string ) | system log default value: "serving.log" |
| log_level | [ LogLevel](#loglevel ) | default value: LogLevel.INFO_LOG_LEVEL |
| max_log_file_size | [ int32](#int32 ) | Byte. default value: 500 * 1024 * 1024 (500MB) |
| max_log_file_count | [ int32](#int32 ) | default value: 10 |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### DPSourceMeta



| Field | Type | Description |
| ----- | ---- | ----------- |
| dm_host | [ string](#string ) | datamesh host. |
| tls_config | [ TlsConfig](#tlsconfig ) | TLS related config. |
 <!-- end Fields -->
 <!-- end HasFields -->



### FileSourceMeta
empty by design

 <!-- end HasFields -->



### HttpSourceMeta



| Field | Type | Description |
| ----- | ---- | ----------- |
| connect_timeout_ms | [ int32](#int32 ) | Max duration for a connect. -1 means wait indefinitely. Default: 60 * 1000 (ms) |
| timeout_ms | [ int32](#int32 ) | Max duration of http request. -1 means wait indefinitely. Default: 120 * 1000 (ms) |
| tls_config | [ TlsConfig](#tlsconfig ) | TLS related config. |
 <!-- end Fields -->
 <!-- end HasFields -->



### ModelConfig
Config for serving model


| Field | Type | Description |
| ----- | ---- | ----------- |
| model_id | [ string](#string ) | Unique id of the model package |
| base_path | [ string](#string ) | Path used to cache and load model package |
| source_path | [ string](#string ) | The path to the model package in the data source, where the content format may vary depending on the `source_type` |
| source_sha256 | [ string](#string ) | Optional. The expect sha256 of the model package |
| source_type | [ SourceType](#sourcetype ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) kind.file_source_meta | [ FileSourceMeta](#filesourcemeta ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) kind.oss_source_meta | [ OSSSourceMeta](#osssourcemeta ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) kind.http_source_meta | [ HttpSourceMeta](#httpsourcemeta ) | none |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) kind.dp_source_meta | [ DPSourceMeta](#dpsourcemeta ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->



### OSSSourceMeta
Options for a Oss model source. Serving accesses data services using the AWS
S3 protocol.


| Field | Type | Description |
| ----- | ---- | ----------- |
| access_key | [ string](#string ) | Bucket access key |
| secret_key | [ string](#string ) | Bucket secret key |
| virtual_hosted | [ bool](#bool ) | Whether to use virtual host mode, ref: https://docs.aws.amazon.com/AmazonS3/latest/userguide/VirtualHosting.html Defalut: False |
| endpoint | [ string](#string ) | none |
| bucket | [ string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### ExponentialBackOffConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| init_ms | [ int32](#int32 ) | The initial backoff time in milliseconds, default 10ms. |
| factor | [ int32](#int32 ) | The backoff time increase factor, should be greater than 1, default 2. |
 <!-- end Fields -->
 <!-- end HasFields -->



### FixedBackOffConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| interval_ms | [ int32](#int32 ) | The backoff time between each retry, in milliseconds, default 10ms. |
 <!-- end Fields -->
 <!-- end HasFields -->



### RandomBackOffConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| min_ms | [ int32](#int32 ) | The minimum backoff time in milliseconds, default 10ms. |
| max_ms | [ int32](#int32 ) | The maximum backoff time in milliseconds, default 50ms. |
 <!-- end Fields -->
 <!-- end HasFields -->



### RetryPolicyConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| retry_custom | [ bool](#bool ) | Default is false, the error code of brpc's retry strategy is used. If it is True, it will be retried on additional error codes, including some http errors (brpc does not retry http errors by default) |
| retry_aggressive | [ bool](#bool ) | Whether to retry on any http or brpc error expect brpc::ERPCTIMEDOUT, default is false. |
| max_retry_count | [ int32](#int32 ) | The number of retries, default 3. |
| backoff_mode | [ RetryPolicyBackOffMode](#retrypolicybackoffmode ) | backoff time mode, default is FIX_BACKOFF. |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) backoff_config.fixed_backoff_config | [ FixedBackOffConfig](#fixedbackoffconfig ) | The fixed backoff time config. |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) backoff_config.exponential_backoff_config | [ ExponentialBackOffConfig](#exponentialbackoffconfig ) | The exponential backoff time config. |
| [**oneof**](https://developers.google.com/protocol-buffers/docs/proto3#oneof) backoff_config.random_backoff_config | [ RandomBackOffConfig](#randombackoffconfig ) | The random backoff time config. |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### ServerConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| feature_mapping | [map ServerConfig.FeatureMappingEntry](#serverconfig-featuremappingentry ) | Optional. Feature name mapping rules. Key: source or predefined feature name. Value: model feature name. |
| tls_config | [ TlsConfig](#tlsconfig ) | Whether to enable tls for server |
| host | [ string](#string ) | e.g. 192.168.2.51 |
| service_port | [ int32](#int32 ) | The port used for model predict service. Default: disable service |
| communication_port | [ int32](#int32 ) | The port used for communication between parties serving. |
| brpc_builtin_service_port | [ int32](#int32 ) | Brpc builtin service listen port. Default: disable service |
| metrics_exposer_port | [ int32](#int32 ) | `/metrics` service listen port. Default: disable service |
| worker_num | [ int32](#int32 ) | Number of pthreads that server runs on. If this option <= 0, use default value. Default: #cpu-cores |
| max_concurrency | [ int32](#int32 ) | Server-level max number of requests processed in parallel. Default: 0 (unlimited) |
| op_exec_worker_num | [ int32](#int32 ) | Number of pthreads that server runs to execute ops. If this option <= 0, use default value. Default: #cpu-cores |
 <!-- end Fields -->
 <!-- end HasFields -->



### ServerConfig.FeatureMappingEntry



| Field | Type | Description |
| ----- | ---- | ----------- |
| key | [ string](#string ) | none |
| value | [ string](#string ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### ServingConfig
Related config of serving


| Field | Type | Description |
| ----- | ---- | ----------- |
| id | [ string](#string ) | Unique id of the serving service |
| server_conf | [ ServerConfig](#serverconfig ) | none |
| model_conf | [ ModelConfig](#modelconfig ) | none |
| cluster_conf | [ ClusterConfig](#clusterconfig ) | none |
| feature_source_conf | [ FeatureSourceConfig](#featuresourceconfig ) | none |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### TlsConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| certificate_path | [ string](#string ) | Certificate file path |
| private_key_path | [ string](#string ) | Private key file path |
| ca_file_path | [ string](#string ) | The trusted CA file to verify the peer's certificate If empty, turn off the verification |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->




### TraceConfig
Serving trace config options


| Field | Type | Description |
| ----- | ---- | ----------- |
| trace_log_enable | [ bool](#bool ) | dump trace to a logfile, default: false |
| trace_log_conf | [ TraceLogConfig](#tracelogconfig ) | if trace_log_enable is true, use this log config to dump trace info |
 <!-- end Fields -->
 <!-- end HasFields -->



### TraceLogConfig



| Field | Type | Description |
| ----- | ---- | ----------- |
| trace_log_path | [ string](#string ) | defaule value: "trace.log" |
| max_trace_log_file_size | [ int32](#int32 ) | Byte. default value: 500 * 1024 * 1024 (500MB) |
| max_trace_log_file_count | [ int32](#int32 ) | default value: 10 |
 <!-- end Fields -->
 <!-- end HasFields -->
 <!-- end messages -->
 <!-- end Files -->

## Enums

 <!-- end Enums -->



### MockDataType


| Name | Number | Description |
| ---- | ------ | ----------- |
| INVALID_MOCK_DATA_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| MDT_RANDOM | 1 | random value for each feature |
| MDT_FIXED | 2 | fixed value for each feature |


 <!-- end Enums -->



### LogLevel
Serving log level

| Name | Number | Description |
| ---- | ------ | ----------- |
| INVALID_LOG_LEVEL | 0 | Placeholder for proto3 default value, do not use it. |
| DEBUG_LOG_LEVEL | 1 | debug |
| INFO_LOG_LEVEL | 2 | info |
| WARN_LOG_LEVEL | 3 | warn |
| ERROR_LOG_LEVEL | 4 | error |


 <!-- end Enums -->



### SourceType
Supported model source type

| Name | Number | Description |
| ---- | ------ | ----------- |
| INVALID_SOURCE_TYPE | 0 | Placeholder for proto3 default value, do not use it. |
| ST_FILE | 1 | Local filesystem |
| ST_OSS | 2 | OSS/AWS S3 |
| ST_HTTP | 3 | Http source |
| ST_DP | 4 | DataProxy source |


 <!-- end Enums -->



### RetryPolicyBackOffMode


| Name | Number | Description |
| ---- | ------ | ----------- |
| INVALID_MODE | 0 | No retry. |
| FIXED_BACKOFF | 1 | Retry with fixed backoff. |
| EXPONENTIAL_BACKOFF | 2 | Retry with exponential backoff. |
| RANDOM_BACKOFF | 3 | Retry with random backoff. |


 <!-- end Enums -->

 <!-- end Enums -->

 <!-- end Enums -->

 <!-- end Enums -->

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
