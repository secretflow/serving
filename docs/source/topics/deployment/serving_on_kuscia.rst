============================
SecretFlow-Serving on Kuscia
============================

SecretFlow-Serving recommends using `Kusica <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans>`_ for deployment and management.

AppImage
========

To deploy SecretFlow-Serving in Kusica, you first need to register the template information for the application image. The specific registration method can be found `here <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/concepts/appimage_cn#id2>`_. The `AppImage` for SecretFlow-Serving is as follows:

.. code-block:: yaml

  apiVersion: kuscia.secretflow/v1alpha1
  kind: AppImage
  metadata:
    name: secretflow-serving-image
  spec:
    configTemplates:
      serving-config.conf: |
        {
          "serving_id": "{{.SERVING_ID}}",
          "input_config": "{{.INPUT_CONFIG}}",
          "cluster_def": "{{.CLUSTER_DEFINE}}",
          "allocated_ports": "{{.ALLOCATED_PORTS}}"
        }
    deployTemplates:
    - name: secretflow
      replicas: 1
      spec:
        containers:
        - command:
          - sh
          - -c
          - ./secretflow_serving --flagfile=conf/gflags.conf --config_mode=kuscia --serving_config_file=/etc/kuscia/serving-config.conf
          configVolumeMounts:
          - mountPath: /etc/kuscia/serving-config.conf
            subPath: serving-config.conf
          name: secretflow
          ports:
          - name: service
            protocol: HTTP
            scope: Domain
          - name: communication
            protocol: HTTP
            scope: Cluster
          - name: internal
            protocol: HTTP
            scope: Domain
          - name: brpc-builtin
            protocol: HTTP
            scope: Domain
          readinessProbe:
            httpGet:
              path: /health
              port: brpc-builtin
          livenessProbe:
            httpGet:
              path: /health
              port: brpc-builtin
          startupProbe:
            failureThreshold: 30
            httpGet:
              path: /health
              port: brpc-builtin
            periodSeconds: 10
            successThreshold: 1
            timeoutSeconds: 1
          workingDir: /root/sf_serving
    image:
      name: secretflow/serving-anolis8
      tag: latest

The explanation of the common fields can be found `here <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/concepts/appimage_cn#appimage-ref>`_.

Other field explanations are as follows:

* `configTemplates`:
    * `serving_id`: Service ID identifier, corresponding to the configuration :ref:`ServingConfig.id <ServingConfig>`. The current content is a placeholder and will actually be replaced by the content in `Kuscia API /api/v1/serving/create <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/apis/serving_cn#create-serving>`_ at startup.
    * `input_config`: SecretFlow-Serving startup configuration, details can be seen in the description below. The current content is a placeholder and will actually be replaced by the content in `Kuscia API /api/v1/serving/create <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/apis/serving_cn#create-serving>`_ at startup.
    * `cluster_def`: See `AppImage explanation <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/concepts/appimage_cn#appimage-ref>`_.
    * `allocated_ports`: See `AppImage explanation <https://www.secretflow.org.cn/docs/kuscia/latest/zh-Hans/reference/concepts/appimage_cn#appimage-ref>`_.

* `ports`:
    * `service`: The :ref:`ServerConfig.service_port <ServerConfig>`
    * `communication`: The :ref:`ServerConfig.communication_port <ServerConfig>`
    * `internal`: The :ref:`ServerConfig.metrics_exposer_port <ServerConfig>`
    * `brpc-builtin`: The :ref:`ServerConfig.brpc_builtin_service_port <ServerConfig>`

Configuration description
=========================

serving_input_config
--------------------

The launch and management of SecretFlow-Serving can be performed using the `Kuscia Serving API <https://www.secretflow.org.cn/docs/kuscia/v0.5.0b0/zh-Hans/reference/apis/serving_cn#serving>`_. In this section, we will explain the contents of the `serving_input_config` field within the `/api/v1/serving/create` request.

.. code-block:: json

  {
    "partyConfigs": {
      "alice": {
        "serverConfig": {
          "featureMapping": {
            "v24": "x24",
            "v22": "x22",
            "v21": "x21",
            "v25": "x25",
            "v23": "x23"
          }
        },
        "modelConfig": {
          "modelId": "glm-test-1",
          "basePath": "/tmp/alice",
          "sourcePath": "examples/alice/glm-test.tar.gz",
          "sourceType": "ST_FILE"
        },
        "featureSourceConfig": {
          "mockOpts": {}
        },
        "channel_desc": {
          "protocol": "http"
        }
      },
      "bob": {
        "serverConfig": {
          "featureMapping": {
            "v6": "x6",
            "v7": "x7",
            "v8": "x8",
            "v9": "x9",
            "v10": "x10"
          }
        },
        "modelConfig": {
          "modelId": "glm-test-1",
          "basePath": "/tmp/bob",
          "sourcePath": "examples/bob/glm-test.tar.gz",
          "sourceType": "ST_FILE"
        },
        "featureSourceConfig": {
          "mockOpts": {}
        },
        "channel_desc": {
          "protocol": "http"
        }
      }
    }
  }

**Field description**:

+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
|                           Name                            |         Type          |                                                                   Description                                                                   |                                Required                                |
+===========================================================+=======================+=================================================================================================================================================+========================================================================+
| partyConfigs                                              | map<str, PartyConfig> | Dictionary of startup parameters for each participant. Key: Participant Unique ID; Value: PartyConfig (Json Object).                            | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.serverConfig                                  | str                   | :ref:`ServerConfig <ServerConfig>`                                                                                                              | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.serverConfig.featureMapping                   | map<str, str>         | Feature name mapping rules. Key: source or predefined feature name; Value: model feature name                                                   | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig                                   | Object                | :ref:`ModelConfig <ModelConfig>`                                                                                                                | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.modelId                           | str                   | Unique id of the model package                                                                                                                  | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.basePath                          | str                   | The local path used to cache and load model package                                                                                             | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.sourcePath                        | str                   | The path to the model package in the data source, where the content format may vary depending on the `sourceType`.                              | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.source_sha256                     | str                   | The expected SHA256 hash of the model package. When provided, the fetched model package will be verified against it.                            | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.sourceType                        | str                   | Model data source type, options include: ST_FILE: In this case, the sourcePath should be a file path accessible to Serving.                     | Yes                                                                    |
|                                                           |                       | ST_DP: In this case, the sourcePath should be DomainData ID in DataMesh from Kuscia. and dpSourceMeta needs to be configured.                   |                                                                        |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.dpSourceMeta                      | Object                | :ref:`DPSourceMeta <DPSourceMeta>`                                                                                                              | No(If `sourceType`  is `DT_DP`, `dpSourceMeta` needs to be configured) |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.modelConfig.dpSourceMeta.dmHost               | str                   | The address of DataMesh in Kuscia. Default: datamesh:8071                                                                                       | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig                           | Object                | :ref:`FeatureSourceConfig <FeatureSourceConfig>`                                                                                                | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.mockOpts                  | Object                | :ref:`MockOptions <MockOptions>`                                                                                                                | No(One of `csvOpts`, `mockOpts`, or `httpOpts` needs to be configured) |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.mockOpts.type             | str                   | The method for generating mock feature values, options: "MDT_RANDOM" for random values, and "MDT_FIXED" for fixed values. Default: "MDT_FIXED". | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.httpOpts                  | Object                | :ref:`HttpOptions <HttpOptions>`                                                                                                                | No(One of `csvOpts`, `mockOpts`, or `httpOpts` needs to be configured) |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.httpOpts.endpoint         | str                   | Feature service address                                                                                                                         | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.httpOpts.enableLb         | bool                  | Whether to enable round robin load balancer, Default: False                                                                                     | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.httpOpts.connectTimeoutMs | int32                 | Max duration for a connect. -1 means wait indefinitely. Default: 500 (ms)                                                                       | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.httpOpts.timeoutMs        | int32                 | Max duration of http request. -1 means wait indefinitely. Default: 1000 (ms)                                                                    | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.csvOpts                   | Object                | :ref:`CsvOptions <CsvOptions>`                                                                                                                  | No(One of `csvOpts`, `mockOpts`, or `httpOpts` needs to be configured) |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.csvOpts.file_path         | Object                | Input file path, specifies where to load data. Note that this will load all of the data into memory at once                                     | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.featureSourceConfig.csvOpts.id_name           | Object                | Id column name, associated with `FeatureParam::query_datas`. `query_datas` is a subset of id column                                             | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.channelDesc                                   | Object                | :ref:`ChannelDesc <ChannelDesc>`                                                                                                                | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.channelDesc.protocol                          | str                   | Communication protocol, for optional value, see `here <https://github.com/apache/brpc/blob/master/docs/en/client.md#protocols>`_                | Yes                                                                    |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.channelDesc.rpcTimeoutMs                      | int32                 | Max duration of RPC. -1 means wait indefinitely. Default: 2000 (ms)                                                                             | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
| PartyConfig.channelDesc.connectTimeoutMs                  | int32                 | Max duration for a connect. -1 means wait indefinitely. Default: 500 (ms)                                                                       | No                                                                     |
+-----------------------------------------------------------+-----------------------+-------------------------------------------------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------+
