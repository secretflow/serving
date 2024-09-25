===============
Feature Service
===============

SecretFlow-Serving recommends providing online prediction feature data by implementing a feature service. To use a feature service as a data source for supporting predictions, you can follow these steps:

Step 1: Implement and deploy the feature service
================================================

SecretFlow-Serving has defined a feature fetching SPI, users need to implement the SPI and deploy it as an http/https service to provide the capability to fetch feature data. The SPI definition can be found :ref:`here <BatchFeatureService>`.

The feature service interface is an HTTP interface where both the request and response bodies are in the *application/json* format. The details are as follows:

| **URL**: http://xxxxx/BatchFeatureService/BatchFetchFeature
| **Method**: POST
| **Headers**: Content-Type: application/json

Example of request body:

.. code-block:: json

    {
      "header": {
        "data": {
          "custom_str": "id_12345"
        }
      },
      "model_service_id": "test_service_id",
      "party_id": "alice",
      "feature_fields": [
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
        "query_datas": [
          "xxxxxxx",
          "yyyyyyy"
        ],
        "query_context": "zzzzzzz"
      }
    }

**Field description**:

+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
|         Name          |         Type         |                                                                                    Description                                                                                     | Required |
+=======================+======================+====================================================================================================================================================================================+==========+
| header                | Object(Header)       | Custom data passed by the Predict request's header.                                                                                                                                | No       |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| header.data           | Map                  | Key:str, Value:str                                                                                                                                                                 | No       |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| model_service_id      | str                  | Model service specification id                                                                                                                                                     | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| party_id              | str                  | Party specification id                                                                                                                                                             | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| feature_fields[]      | List(FeatureField)   | Request feature field list                                                                                                                                                         | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| feature_fields[].name | str                  | Unique name of the feature                                                                                                                                                         | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| feature_fields[].type | enum(FieldType)      | Field type of the feature                                                                                                                                                          | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| param                 | Object(FeatureParam) | Custom parameters data for querying feature values, the specific content format of which is determined by the feature datasource provider. Each one for query one row of features. | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| param.query_datas[]   | List(str)            | The serialized datas for query features. Each one for query one row of features.                                                                                                   | Yes      |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+
| param.query_context   | str                  | Optional. Represents the common part of the query datas.                                                                                                                           | No       |
+-----------------------+----------------------+------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+----------+

Example of response body:

.. code-block:: json

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

**Field description**:

+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
|          Name           |         Type         |            Description            |                            Required                             |
+=========================+======================+===================================+=================================================================+
| header                  | Object(Header)       | Custom data                       | No                                                              |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| header.data             | Map                  | Key:str, Value:str                | No                                                              |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| status                  | Object(Status)       | The Status of this response       | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| status.code             | enum(ErrorCode)      | Value reference ``ErrorCode``     | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| status.msg              | str                  | The detail message of the status  | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features                | List                 | The Request feature data list     | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].field        | Object(FeatureField) | The definition of a feature field | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].field.name   | str                  | Unique name of the feature        | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].field.type   | enum(FieldType)      | Field type of the feature         | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value        | Object(FeatureValue) | The definition of a feature value | Yes                                                             |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.i32s[] | List                 | int32 feature value data list     | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.i64s[] | List                 | int64 feature value data list     | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.fs[]   | List                 | float feature value data list     | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.ds[]   | List                 | double feature value data list    | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.ss[]   | List                 | string feature value data list    | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+
| features[].value.bs[]   | List                 | bool feature value data list      | No(assign the corresponding value list based on ``field.type``) |
+-------------------------+----------------------+-----------------------------------+-----------------------------------------------------------------+


Step 2: Configure startup config
================================

Configure the feature data source in the SecretFlow-Serving startup configuration to the following pattern:

.. code-block:: json

  {
    "featureSourceConf": {
      "httpOpts": {
        "endpoint": "http://xxxxx/BatchFeatureService/BatchFetchFeature"
      }
    }
  }

In this configuration, the endpoint is the URL of the feature service's feature fetching interface. SecretFlow-Serving will access the feature service to obtain feature data according to the SPI definition. Detailed configuration instructions can be found :ref:`here <HttpOptions>`.
