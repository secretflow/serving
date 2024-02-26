====================================
How to deployment SecretFlow-Serving
====================================

This document describes how to deploy SecretFlow-Serving with docker, it's basically same with :doc:`/intro/tutorial`, but deployed in multi-machine.

Before start this doc, we assume that the reader has some experience using the docker-compose utility. If you are new to Docker Compose, please consider reviewing the `official Docker Compose overview <https://docs.docker.com/compose/>`_, or checking out the `Getting Started guide <https://docs.docker.com/compose/gettingstarted/>`_.

Deployment Diagram
==================

The deployment diagram of the SecretFlow-Serving system that we plan to deploy is shown as the following figure, it involves a total of two party, including two parties named ``Alice`` and ``Bob``. We use two machines to simulate different parties.

.. image:: /imgs/architecture.png

.. note::
    1. The SecretFlow-Serving is served through the HTTP protocol. It is recommended to use HTTPS instead in production environments. Please check :ref:`TLS Configuration <TlsConfig>` for details.

Step 1: Deploy SecretFlow-Serving
=================================

Here we present how to deploy serving in party ``Alice``, it's same with party ``Bob``.

1.1 Create a Workspace
-----------------------

.. code-block:: bash

    mkdir serving
    cd serving

Here, we use the model file from the "examples" directory as a demonstration and place it in the "serving" directory. Please replace the following path with the actual path according to your situation.

.. code-block:: bash

    cp serving/examples/alice/glm-test.tar.gz .


.. note::

  For ``Bob`` should use model file `serving/examples/bob/glm-test.tar.gz`.


1.2 Create Serving config file
------------------------------

Create a file called ``serving.config`` in your workspace and paste the following code in:

.. code-block:: json

  {
    "id": "test_service_id",
    "serverConf": {
      "featureMapping": {
        "v24": "x24",
        "v22": "x22",
        "v21": "x21",
        "v25": "x25",
        "v23": "x23"
      },
      "metricsExposerPort": 10306,
      "brpcBuiltinServicePort": 10307
    },
    "modelConf": {
      "modelId": "glm-test",
      "basePath": "./data",
      "sourcePath": "./glm-test.tar.gz",
      "sourceSha256": "3b6a3b76a8d5bbf0e45b83f2d44772a0a6aa9a15bf382cee22cbdc8f59d55522",
      "sourceType": "ST_FILE"
    },
    "clusterConf": {
      "selfId": "alice",
      "parties": [
        {
          "id": "alice",
          "address": "0.0.0.0:9010"
        },
        {
          "id": "bob",
          "address": "0.0.0.0:9011"
        }
      ],
      "channel_desc": {
        "protocol": "baidu_std"
      }
    },
    "featureSourceConf": {
      "mockOpts": {}
    }
  }

See :ref:`Serving Config <ServingConfig>` for more config information

.. note::

  * The above configuration is referenced from `alice-serving-config <https://github.com/secretflow/serving/blob/main/examples/alice/serving.config>`_.
  * For ``Bob``, you should refer to `bob-serving-config <https://github.com/secretflow/serving/blob/main/examples/bob/serving.config>`_ .
  * **Note** `mockOpts` is configured here for a mock feature data source, which will provide simulated feature data to support the model prediction process. In actual use, please provide feature data in the form of a feature service by implementing the :ref:`SecretFlow-Serving Feature Service SPI <BatchFeatureService>`.

.. _log_conf_file:

1.3 Create logging config file
------------------------------

Create a file called ``logging.config`` in your workspace and paste the following code in:

.. code-block:: json

  {
    "systemLogPath": "./serving.log",
    "logLevel": 2,
    "maxLogFileSize": 104857600,
    "maxLogFileCount": 2
  }

See :ref:`Logging Config <LoggingConfig>` for more logging config information.

.. note::

  The above configuration is referenced from `alice-logging-config <https://github.com/secretflow/serving/blob/main/examples/alice/logging.config>`_.

  For ``Bob``, you should refer to `bob-logging-config <https://github.com/secretflow/serving/blob/main/examples/bob/logging.config>`_ .


1.4 Create docker-compose file
------------------------------

Create a file called ``docker-compose.yaml`` in your workspace and paste the following code in:

.. code-block:: yaml

  version: "3.8"
  services:
    serving:
      cap_add:
        - NET_ADMIN
      command:
        - /root/sf_serving/secretflow_serving
        - --serving_config_file=/root/sf_serving/conf/serving.config
        - --logging_config_file=/root/sf_serving/conf/logging.config
      restart: always
      image: secretflow/serving-anolis8:latest
      ports:
        - __ALICE_PORT__:9010
      volumes:
        - ./serving.conf:/root/sf_serving/conf/serving.config

.. note::

  ``__ALICE_PORT__``  is the published port on the host machine which is used for SecretFlow-Serving service to listen on, you need to replace it with an accessible port number. In this case, we have designated it as ``9010`` for ``Alice``, ``9011`` for ``Bob``.


Step 2: Start Serving Service
=============================

The file your workspace should be as follows:

.. code-block:: bash

  └── serving
    ├── serving.config
    ├── logging.config
    └── docker-compose.yaml

Then you can start serving service by running docker compose up

.. code-block:: bash

  # If you install docker with Compose V1, pleas use `docker-compose` instead of `docker compose`
  docker compose -f docker-compose.yaml up -d

You can use docker logs to check whether serving works well

.. code-block:: bash

  docker logs -f serving_serving_1

Now, ``Alice`` serving is listening on ``9010``, you can confirm if the service is ready by accessing the ``/health`` endpoint.

.. code-block:: bash

  curl --location 'http://127.0.0.1:9010/health'

When the endpoint returns a status code of ``200``, it means that the service is ready.

Step 3: Predict Test
====================

Based on the capabilities of `Brpc <https://brpc.apache.org/docs/server/>`_, serving supports accessing through various protocols. Here, we are using an HTTP request to test the predict interface of serving.

You can read :ref:`SecretFlow-Serving API <PredictionService>` for more information about serving APIs.

.. code-block:: bash

  curl --location 'http://127.0.0.1:9010/PredictionService/Predict' \
      --header 'Content-Type: application/json' \
      --data '{
          "service_spec": {
              "id": "test_service_id"
          },
          "fs_params": {
              "alice": {
                  "query_datas": [
                      "a"
                  ]
              },
              "bob": {
                  "query_datas": [
                      "a"
                  ]
              }
          }
      }'
