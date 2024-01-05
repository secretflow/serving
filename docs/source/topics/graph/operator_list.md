



SecretFlow-Serving Operator List
================================


Last update: Thu Dec 28 14:28:43 2023
## MERGE_Y


Operator version: 0.0.2

Merge all partial y(score) and apply link function
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|output_col_name|The column name of merged score|String|Y||
|link_function|Type of link function, defined in `secretflow_serving/protos/link_function.proto`. Optional value: LF_LOG, LF_LOGIT, LF_INVERSE, LF_RECIPROCAL, LF_IDENTITY, LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, LF_SIGMOID_GA, LF_SIGMOID_T1, LF_SIGMOID_T3, LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, LF_SIGMOID_SEG3, LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS|String|Y||
|input_col_name|The column name of partial_y|String|Y||
|yhat_scale|In order to prevent value overflow, GLM training is performed on the scaled y label. So in the prediction process, you need to enlarge yhat back to get the real predicted value, `yhat = yhat_scale * link(X * W)`|Double|N|Default: 1.0.|

### Tags


|Name|Description|
| :--- | :--- |
|returnable|The operator's output can be the final result|
|mergeable|The operator accept the output of operators with different participants and will somehow merge them.|

### Inputs


|Name|Description|
| :--- | :--- |
|partial_ys|The list of partial y, data type: `double`|

### Output


|Name|Description|
| :--- | :--- |
|scores|The merge result of `partial_ys`, data type: `double`|

## DOT_PRODUCT


Operator version: 0.0.2

Calculate the dot product of feature weights and values
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|intercept|Value of model intercept|Double|N|Default: 0.0.|
|output_col_name|Column name of partial y|String|Y||
|feature_weights|List of feature weights|Double List|Y||
|input_types|List of input feature data types, Note that there is a loss of precision when using `DT_FLOAT` type. Optional value: DT_UINT8, DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, DT_INT64, DT_FLOAT, DT_DOUBLE|String List|Y||
|feature_names|List of feature names|String List|Y||

### Inputs


|Name|Description|
| :--- | :--- |
|features|Input feature table|

### Output


|Name|Description|
| :--- | :--- |
|partial_ys|The calculation results, they have a data type of `double`.|

## ARROW_PROCESSING


Operator version: 0.0.1

Replay secretflow compute functions
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|content_json_flag|Whether `trace_content` is serialized json|Boolean|N|Default: False.|
|trace_content|Serialized data of secretflow compute trace|Bytes|N||
|output_schema_bytes|Serialized data of output schema(arrow::Schema)|Bytes|Y||
|input_schema_bytes|Serialized data of input schema(arrow::Schema)|Bytes|Y||

### Tags


|Name|Description|
| :--- | :--- |
|returnable|The operator's output can be the final result|

### Inputs


|Name|Description|
| :--- | :--- |
|input||

### Output


|Name|Description|
| :--- | :--- |
|output||
