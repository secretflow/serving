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
    1. The SecretFlow-Serving in this example provides services through the `HTTP` protocol. However, for a production environment, it is recommended to use `mTLS` to secure all communication links and access the service via `HTTPS`. Please check :ref:`mTLS Configuration <TlsConfig>` for details.
    2. SecretFlow-Serving supports configuring different `mTLS` settings for various communication links. For more detailed information, please refer to the :ref:`Server Configuration <ServingConfig>`, :ref:`Cluster Communication Configuration <ClusterConfig>`, and :ref:`SPI Communication Configuration <HttpOptions>`.

Step 1: Deploy SecretFlow-Serving
=================================

Here we present how to deploy serving in party ``Alice``, it's same with party ``Bob``.

1.1 Create a Workspace
-----------------------

.. code-block:: bash

    mkdir serving
    cd serving

Here, we use the model file from the "examples" directory as a demonstration and place it in the "serving" directory. Please replace the following path with the actual path according to your situation.

Model packages for SecretFlow-Serving can be generated by using the `model_export` component of SecretFlow. For more details, please refer to `model_export <https://www.secretflow.org.cn/en/docs/secretflow/component/comp_list#model_export>`_.

.. code-block:: bash

    mkdir model
    cp serving/examples/alice/glm-test.tar.gz ./model


.. note::

  For ``Bob`` should use model file `serving/examples/bob/glm-test.tar.gz`.


1.2 Create Serving config file
------------------------------

.. code-block:: bash

    mkdir conf


Create a file called ``serving.config`` in the ``conf`` directory within your workspace and paste the following code in:

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
      "host": "0.0.0.0",
      "servicePort": "9010",
      "communicationPort": "9110",
      "metricsExposerPort": 10306,
      "brpcBuiltinServicePort": 10307
    },
    "modelConf": {
      "modelId": "glm-test",
      "basePath": "./data",
      "sourcePath": "./model/glm-test.tar.gz",
      "sourceSha256": "c6308af488bcd6c54a48a145af17aa209dec463b5cb44d83c6b58195818c10a0",
      "sourceType": "ST_FILE"
    },
    "clusterConf": {
      "selfId": "alice",
      "parties": [
        {
          "id": "alice",
          "address": "__ALICE_URL__"
        },
        {
          "id": "bob",
          "address": "__BOB_URL__"
        }
      ],
      "channelDesc": {
        "protocol": "http"
      }
    },
    "featureSourceConf": {
      "mockOpts": {}
    }
  }

See :ref:`Serving Config <ServingConfig>` for more config information

.. note::

  * For Alice, ``__ALICE_URL__`` can be simply configured to ``0.0.0.0:9110``, ``__BOB_URL__`` is the url (eg:http://192.168.11.34:9111) where bob's serving ``communicationPort`` is listen on, you need to replace it with bob's serving url. 
  * The above configuration is referenced from `alice-serving-config <https://github.com/secretflow/serving/blob/main/examples/alice/serving.config>`_. Please adjust the configuration items according to your actual scenario.
  * **Note** `mockOpts` is configured here for a mock feature data source, which will provide simulated feature data to support the model prediction process. In actual use, please provide feature data in the form of a feature service by implementing the :ref:`SecretFlow-Serving Feature Service SPI <BatchFeatureService>`.

.. _log_conf_file:

1.3 Create logging config file
------------------------------

Create a file called ``logging.config`` in the ``conf`` directory within your workspace and paste the following code in:

.. code-block:: json

  {
    "systemLogPath": "./serving.log",
    "logLevel": 2,
    "maxLogFileSize": 104857600,
    "maxLogFileCount": 2
  }

See :ref:`Logging Config <LoggingConfig>` for more logging config information.

.. note::

  The above configuration is referenced from `alice-logging-config <https://github.com/secretflow/serving/blob/main/examples/alice/logging.config>`_. Please adjust the configuration items according to your actual scenario.

.. _trace_config_file:

1.4 Create trace config file
------------------------------

This configuration is optional, and **may cause some performance degradation**, please use it with caution.
To record trace info, you can create a file called ``trace.config`` in the ``conf`` directory within your workspace and paste the following code in:

.. code-block:: json

  {
    "traceLogEnable": true,
    "traceLogConf": {
      "traceLogPath": "./trace.log"
    }
  }

See :ref:`Trace Config <TraceConfig>` and :ref:`Trace Introduction <Trace>` for more trace config information.

.. note::

  The above configuration is referenced from `alice-trace-config <https://github.com/secretflow/serving/blob/main/examples/alice/trace.config>`_. Please adjust the configuration items according to your actual scenario.


1.5 Create docker-compose file
------------------------------

Create a file called ``docker-compose.yaml`` in your workspace and paste the following code in:

.. code-block:: yaml

  version: "3.8"
  services:
    serving:
      command:
        - /root/sf_serving/secretflow_serving
        - --serving_config_file=/root/sf_serving/conf/serving.config
        - --logging_config_file=/root/sf_serving/conf/logging.config
        - --trace_config_file=/root/sf_serving/conf/trace.config
      restart: always
      image: secretflow/serving-anolis8:latest
      ports:
        - __SVC_PORT__:9010
        - __COM_PORT__:9110
      volumes:
        - ./conf:/root/sf_serving/conf
        - ./glm-test.tar.gz:/root/sf_serving/model/glm-test.tar.gz

.. note::

  * ``__SVC_PORT__``  is the published port on the host machine which is used for SecretFlow-Serving prediction service to listen on, you need to replace it with an accessible port number. In this case, we designate it as ``9010`` for ``Alice``, ``9011`` for ``Bob``.
  * ``__COM_PORT__``  is the published port on the host machine which is used for SecretFlow-Serving communication services to listen on, you need to replace it with an accessible port number. In this case, we designate it as ``9110`` for ``Alice``, ``9111`` for ``Bob``.
  * ``trace_config_file`` line is optional.
  * You can also use mirrors at Alibaba Cloud: ``secretflow-registry.cn-hangzhou.cr.aliyuncs.com/secretflow/serving-anolis8:latestsecretflow-registry.cn-hangzhou.cr.aliyuncs.com/secretflow/serving-anolis8:[tag]``


Step 2: Start Serving Service
=============================

Here, we'll use starting Alice's service as an example. Similarly, you will also need to start the services for the other prediction participants.

The file your workspace should be as follows, ``trace.config`` is optional:

.. code-block:: bash

  └── serving
    ├── docker-compose.yaml
    ├── glm-test.tar.gz
    └── conf
      ├── serving.config
      ├── logging.config
      └── trace.config

Then you can start serving service by running docker compose up.

.. code-block:: bash

  # If you install docker with Compose V1, pleas use `docker-compose` instead of `docker compose`
  docker compose -p alice-serving -f docker-compose.yaml up -d

You can use docker logs to check whether serving works well

.. code-block:: bash

  docker logs -f alice-serving-serving-1

Now, ``Alice`` prediction serving is listening on ``9010``, you can confirm if the service is ready by accessing the ``/health`` endpoint.

.. code-block:: bash

  curl --location 'http://127.0.0.1:9010/health'

When the endpoint returns a status code of ``200``, it means that the service is ready.

.. note::

  The Secretflow-Serving cluster can start providing model prediction services only after all participants' services have been started and are ready.

Step 3: Predict Test
====================

Based on the capabilities of `Brpc <https://brpc.apache.org/docs/server/>`_, serving supports accessing through various protocols. Here, we are using an HTTP request to test the predict interface of serving.

You can read :ref:`SecretFlow-Serving API <PredictionService>` for more information about serving APIs.

.. code-block:: bash

  curl --location 'http://127.0.0.1:9010/PredictionService/Predict' \
      --header 'Content-Type: application/json' \
      --header 'X-B3-TraceId: 463ac35c9f6413ad48485a3953bb' \
      --header 'X-B3-SpanId: a2fb4a1d1a96d312' \
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
