



SecretFlow-Serving Operator List
================================


Last update: Tue Nov 19 17:28:18 2024
## PHE_2P_TREE_SELECT


Operator version: 0.0.1

Obtaining the local prediction path information of the decision tree using input features.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|split_values|node split value, goes left when less than it. valid when `is_leaf == false`|Double List|Y||
|split_feature_idxs|The list of split feature index, `-1` means feature not belong to party or not valid|Integer32 List|Y||
|lchild_ids|The left child node id list, `-1` means not valid|Integer32 List|Y||
|leaf_node_ids|The leaf node ids list.|Integer32 List|Y||
|rchild_ids|The right child node id list, `-1` means not valid|Integer32 List|Y||
|root_node_id|The id of the root tree node|Integer32|N|Default: 0.|
|weight_shard_col_name|Column name of weight shard|String|Y||
|select_col_name|Column name of tree select|String|Y||
|input_feature_types|List of input feature data types. Optional value: DT_UINT8, DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, DT_INT64, DT_FLOAT, DT_DOUBLE|String List|Y||
|weight_shard|The leaf node's weight shard list.The order must remain consistent with the sequence in `leaf_node_ids`.|Bytes|Y||
|node_ids|The id list of the tree node|Integer32 List|Y||
|input_feature_names|List of feature names|String List|Y||

### Inputs


|Name|Description|
| :--- | :--- |
|features|Input feature table|

### Output


|Name|Description|
| :--- | :--- |
|selects|The local prediction path information of the decision tree.|

## TREE_SELECT


Operator version: 0.0.1

Obtaining the local prediction path information of the decision tree using input features.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|split_values|node split value, goes left when less than it. valid when `is_leaf == false`|Double List|Y||
|split_feature_idxs|The list of split feature index, `-1` means feature not belong to party or not valid|Integer32 List|Y||
|lchild_ids|The left child node id list, `-1` means not valid|Integer32 List|Y||
|leaf_node_ids|The leaf node ids list. The order must remain consistent with the sequence in `TREE_MERGE.leaf_weights`.|Integer32 List|Y||
|rchild_ids|The right child node id list, `-1` means not valid|Integer32 List|Y||
|root_node_id|The id of the root tree node|Integer32|N|Default: 0.|
|output_col_name|Column name of tree select|String|Y||
|input_feature_types|List of input feature data types. Optional value: DT_UINT8, DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, DT_INT64, DT_FLOAT, DT_DOUBLE|String List|Y||
|node_ids|The id list of the tree node|Integer32 List|Y||
|input_feature_names|List of feature names|String List|Y||

### Inputs


|Name|Description|
| :--- | :--- |
|features|Input feature table|

### Output


|Name|Description|
| :--- | :--- |
|select|The local prediction path information of the decision tree.|

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

## PHE_2P_DOT_PRODUCT


Operator version: 0.0.1

Two-party computation operator. Load the encrypted feature weights, compute their dot product with the feature values, and add random noise to the result for obfuscation. Only supports computation between two parties, with the weights being encrypted using the other party's key.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|result_col_name|The name of the calculation result(partial_y) column in the output|String|Y||
|offset_col_name|The name of the offset column(feature) in the input|String|N|Default: .|
|rand_number_col_name|The name of the generated rand number column in the output|String|Y||
|feature_types|List of input feature data types. Optional value: DT_UINT8, DT_INT8, DT_UINT16, DT_INT16, DT_UINT32, DT_INT32, DT_UINT64, DT_INT64, DT_FLOAT, DT_DOUBLE|String List|N|Default: [].|
|feature_weights_ciphertext|feature weight ciphertext matrix bytes|Bytes|N||
|intercept_ciphertext|Intercept ciphertext bytes or matrix bytes|Bytes|N||
|feature_names|List of feature names. Note that if there is an offset column, it needs to be the last one in the list|String List|N|Default: [].|

### Inputs


|Name|Description|
| :--- | :--- |
|features|Input features|

### Output


|Name|Description|
| :--- | :--- |
|partial_y|Calculation results|

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

## TREE_ENSEMBLE_PREDICT


Operator version: 0.0.2

Accept the weighted results from multiple trees (`TREE_SELECT` + `TREE_MERGE`), merge them, and obtain the final prediction result of the tree ensemble.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|base_score|The initial prediction score, global bias.|Double|N|Default: 0.0.|
|num_trees|The number of ensemble's tree|Integer32|Y||
|output_col_name|The column name of tree ensemble predict score|String|Y||
|algo_func|Optional value: LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, LF_SIGMOID_GA, LF_SIGMOID_T1, LF_SIGMOID_T3, LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, LF_SIGMOID_SEG3, LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS|String|N|Default: LF_IDENTITY.|
|input_col_name|The column name of tree weight|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|returnable|The operator's output can be the final result|

### Inputs


|Name|Description|
| :--- | :--- |
|*args|variable inputs, accept tree's weights|

### Output


|Name|Description|
| :--- | :--- |
|score|The prediction result of tree ensemble.|

## PHE_2P_DECRYPT_PEER_Y


Operator version: 0.0.1

Two-party computation operator. Decrypt the obfuscated partial_y and add a random number.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|decrypted_col_name|The name of the decrypted result column in the output|String|Y||
|partial_y_col_name|The name of the partial_y(which can be decrypt by self) column in the input|String|Y||

### Inputs


|Name|Description|
| :--- | :--- |
|crypted_data|Input feature table|

### Output


|Name|Description|
| :--- | :--- |
|decrypted_data|Decrypted partial_y with the added random number.|

## MERGE_Y


Operator version: 0.0.3

Merge all partial y(score) and apply link function
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|exp_iters|Number of iterations of `exp` approximation, valid when `link_function` set `LF_EXP_TAYLOR`|Integer32|N|Default: 0.|
|output_col_name|The column name of merged score|String|Y||
|link_function|Type of link function, defined in `secretflow_serving/protos/link_function.proto`. Optional value: LF_EXP, LF_EXP_TAYLOR, LF_RECIPROCAL, LF_IDENTITY, LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, LF_SIGMOID_GA, LF_SIGMOID_T1, LF_SIGMOID_T3, LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, LF_SIGMOID_SEG3, LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS|String|Y||
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

## PHE_2P_TREE_ENSEMBLE_MERGE


Operator version: 0.0.1

Accept the weighted results from multiple trees, then merge them.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|num_trees|The number of ensemble's tree|Integer32|Y||
|output_col_name|The column name of the tree ensemble encrypted raw predict score|String|Y||
|input_col_name|The column name of tree weight|String|Y||

### Inputs


|Name|Description|
| :--- | :--- |
|*args|variable inputs, accept tree's weights|

### Output


|Name|Description|
| :--- | :--- |
|merged_raw_score|The merged raw result of tree ensemble.|

## PHE_2P_MERGE_Y


Operator version: 0.0.1

Two-party computation operator. Merge the obfuscated partial_y decrypted by the peer party with the partial_y based on self own key to obtain the final prediction score.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|exp_iters|Number of iterations of `exp` approximation, valid when `link_function` set `LF_EXP_TAYLOR`|Integer32|N|Default: 0.|
|link_function|Type of link function, defined in `secretflow_serving/protos/link_function.proto`. Optional value: LF_EXP, LF_EXP_TAYLOR, LF_RECIPROCAL, LF_IDENTITY, LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, LF_SIGMOID_GA, LF_SIGMOID_T1, LF_SIGMOID_T3, LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, LF_SIGMOID_SEG3, LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS|String|Y||
|yhat_scale|In order to prevent value overflow, GLM training is performed on the scaled y label. So in the prediction process, you need to enlarge yhat back to get the real predicted value, `yhat = yhat_scale * link(X * W)`|Double|N|Default: 1.0.|
|score_col_name|The name of the score column in the output|String|Y||
|crypted_y_col_name|The name of the crypted partial_y column in the second input|String|Y||
|decrypted_y_col_name|The name of the decrypted partial_y column in the first input|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|returnable|The operator's output can be the final result|

### Inputs


|Name|Description|
| :--- | :--- |
|crypted_data|The crypted data selected by `PHE_2P_REDUCE`|
|decrypted_data|The decrypted data output by `PHE_2P_DECRYPT_PEER_Y`|

### Output


|Name|Description|
| :--- | :--- |
|score|The final linear predict score.|

## TREE_MERGE


Operator version: 0.0.1

Merge the `TREE_SELECT` output from multiple parties to obtain a unique prediction path and return the result weights.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|leaf_weights|The weight list for leaf node, If party does not possess weights. The attr can be omitted.|Double List|N|Default: [].|
|output_col_name|The column name of tree predict score|String|Y||
|input_col_name|The column name of selects|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|mergeable|The operator accept the output of operators with different participants and will somehow merge them.|

### Inputs


|Name|Description|
| :--- | :--- |
|selects|Input tree selects|

### Output


|Name|Description|
| :--- | :--- |
|score|The prediction result of tree.|

## PHE_2P_REDUCE


Operator version: 0.0.1

Two-party computation operator. Select data encrypted by either our side or the peer party according to the configuration.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|select_crypted_for_peer|If `True`, select the data can be decrypted by peer, including self calculated partial_y and peer's rand, otherwise select selfs.|Boolean|Y||
|rand_number_col_name|The name of the rand number column in the input and output|String|Y||
|partial_y_col_name|The name of the partial_y column in the input and output|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|mergeable|The operator accept the output of operators with different participants and will somehow merge them.|

### Inputs


|Name|Description|
| :--- | :--- |
|compute results|The compute results from both self and peer's|

### Output


|Name|Description|
| :--- | :--- |
|selected results|The selected data|

## PHE_2P_TREE_ENSEMBLE_PREDICT


Operator version: 0.0.1

Decrypt and calculate the prediction score of the tree ensemble
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|base_score|The initial prediction score, global bias.|Double|N|Default: 0.0.|
|output_col_name|The column name of the ensemble predict score|String|Y||
|algo_func|Optional value: LF_SIGMOID_RAW, LF_SIGMOID_MM1, LF_SIGMOID_MM3, LF_SIGMOID_GA, LF_SIGMOID_T1, LF_SIGMOID_T3, LF_SIGMOID_T5, LF_SIGMOID_T7, LF_SIGMOID_T9, LF_SIGMOID_LS7, LF_SIGMOID_SEG3, LF_SIGMOID_SEG5, LF_SIGMOID_DF, LF_SIGMOID_SR, LF_SIGMOID_SEGLS|String|N|Default: LF_IDENTITY.|
|input_col_name|The column name of the tree ensemble encrypted raw predict score|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|returnable|The operator's output can be the final result|

### Inputs


|Name|Description|
| :--- | :--- |
|raw_score|The encryped raw score of the tree ensemble|

### Output


|Name|Description|
| :--- | :--- |
|score|The prediction result of the tree ensemble.|

## PHE_2P_TREE_MERGE


Operator version: 0.0.1

Merge the `PHE_2P_TREE_SELECT` output from multiple parties to obtain a unique prediction path and return the tree weight.
### Attrs


|Name|Description|Type|Required|Notes|
| :--- | :--- | :--- | :--- | :--- |
|weight_shard|The leaf node's weight shard list.The order must remain consistent with the sequence in `PHE_2P_TREE_SELECT.leaf_node_ids`.|Bytes|Y||
|output_col_name|The column name of tree predict score|String|Y||
|weight_shard_col_name|The column name of weight shard|String|Y||
|select_col_name|The column name of selects|String|Y||

### Tags


|Name|Description|
| :--- | :--- |
|mergeable|The operator accept the output of operators with different participants and will somehow merge them.|

### Inputs


|Name|Description|
| :--- | :--- |
|selects|Input tree selects|

### Output


|Name|Description|
| :--- | :--- |
|tree_weight|The final weight of the tree.|
