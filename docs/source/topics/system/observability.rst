SecretFlow-Serving System Observability
=======================================

Secretflow-Serving currently supports two observation types: logs and metrics.

Logs
======

You can configure the log path, log level, etc. by specifying the :ref:`LoggingConfig` when serving is started.
You can also view the :ref:`example <log_conf_file>`.

Metrics
========

Format
----------

Secretflow-Serving uses the `Prometheus <https://prometheus.io/>`_ standard to generate metrics.
The metric service is turned off by default,
you may start metric service by specifying ``metrics_exposer_port`` of :ref:`ServerConfig <SecretFlow-Serving Config>`.
Then You can obtain the metrics by requesting :ref:`MetricsService <metrics>` on this port.
That is to say, Serving supports pull mode.
You could use `The Prometheus monitoring system <https://prometheus.io/>`_ to collect metrics,
or simply use ``curl`` like this:

.. code-block:: shell

    curl xx.xx.xx.xx:port/metrics

Metric entries
------------------

Serving mainly records the number of interface requests and the request duration time for various services,
with some additional labels,
such as the error code, party_id, etc.

The sevices of Secretflow-Serving are shown below:

.. image:: /imgs/services.png


:ref:`PredictRequest` first goes to the :ref:`PredictionService`,
and the :ref:`PredictionService` will then request the local ``ExecutionCore`` or
the remote :ref:`ExecutionService` according to the different :ref:`DispatchType`  in
the Graph. If the :ref:`FeatureSourceType` of request is ``FS_SERVICE``,
then the ``ExecutionCore`` will request the :ref:`BatchFeatureService`.

:ref:`GetModelInfoRequest` goes to :ref:`ModelService` to get model info. :ref:`Model info <ModelInfo>` is public for all parties.

Metrics of Secretflow-Serving have the following parts:

.. note::
    Prometheus supports a multi-dimensional data model with time series data identified by metric name and key/value pairs, called labels.
    Secretflow-Serving metrics have some common labels:

    1. handler: the subject providing services
    2. action: operation name
    3. party id: ``self_id`` of :ref:`ClusterConfig`
    4. service_id: ``id`` of :ref:`ServingConfig`

    If you want to know what is ``Counter`` or ``Summary``, you could check out `this page <https://prometheus.io/docs/concepts/metric_types/>`_.


Brpc metric
^^^^^^^^^^^^^^^^^

Serving will dump brpc internal metrics in Prometheus format, refer to `issue <https://github.com/apache/brpc/issues/1322>`_.

MetricsService metrics
^^^^^^^^^^^^^^^^^^^^^^

+---------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+
|                 name                  |  type   |                                                       desc                                                        |
+=======================================+=========+===================================================================================================================+
| exposer_transferred_bytes_total       | Counter | Transferred bytes to metrics services                                                                             |
+---------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+
| exposer_scrapes_total                 | Counter | Number of times metrics were scraped                                                                              |
+---------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+
| exposer_request_duration_milliseconds | Summary | Summary of latencies of serving scrape requests, in milliseconds  with  0.5-quantile, 0.9-quantile, 0.99-quantile |
+---------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+


PredictionService metrics
^^^^^^^^^^^^^^^^^^^^^^^^^

+------------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+------------------------------+
|                   name                   |  type   |                                                       desc                                                        |            label             |
+==========================================+=========+===================================================================================================================+==============================+
| prediction_request_count                 | Counter | How many prediction service api requests are handled by this server.                                              | handler: PredictionService   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | service_id                   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | party_id                     |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | action                       |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | code: error code of response |
+------------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+------------------------------+
| prediction_sample_count                  | Counter | How many prediction samples are processed by this services.                                                       | handler: PredictionService   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | service_id                   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | party_id                     |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | action                       |
+------------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+------------------------------+
| prediction_request_duration_milliseconds | Summary | Summary of prediction service api request duration in milliseconds with 0.5-quantile, 0.9-quantile, 0.99-quantile | handler: PredictionService   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | service_id                   |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | party_id                     |
+                                          +         +                                                                                                                   +------------------------------+
|                                          |         |                                                                                                                   | action                       |
+------------------------------------------+---------+-------------------------------------------------------------------------------------------------------------------+------------------------------+


ExecutionService metrics
^^^^^^^^^^^^^^^^^^^^^^^^
+-----------------------------------------+---------+--------------------------------------------------------------------------------------------------------------------+------------------------------+
|                  name                   |  type   |                                                        desc                                                        |            labels            |
+=========================================+=========+====================================================================================================================+==============================+
| execution_request_count                 | Counter | How many execution requests are handled by this server.                                                            | handler: ExecutionService    |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | service_id                   |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | party_id                     |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | action                       |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | code: error code of response |
+-----------------------------------------+---------+--------------------------------------------------------------------------------------------------------------------+------------------------------+
| execution_request_duration_milliseconds | Summary | Summary of execution service api request duration in milliseconds  with  0.5-quantile, 0.9-quantile, 0.99-quantile | handler: ExecutionService    |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | service_id                   |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | party_id                     |
+                                         +         +                                                                                                                    +------------------------------+
|                                         |         |                                                                                                                    | action                       |
+-----------------------------------------+---------+--------------------------------------------------------------------------------------------------------------------+------------------------------+

ModelService metrics
^^^^^^^^^^^^^^^^^^^^^

+---------------------------------------------+---------+----------------------------------------------------------------------------------------------------------------+------------------------------+
|                    name                     |  type   |                                                      desc                                                      |            labels            |
+=============================================+=========+================================================================================================================+==============================+
| model_service_request_count                 | Counter | How many execution requests are handled by this server.                                                        | handler: ModelService        |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | service_id                   |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | party_id                     |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | action                       |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | code: error code of response |
+---------------------------------------------+---------+----------------------------------------------------------------------------------------------------------------+------------------------------+
| model_service_request_duration_milliseconds | Summary | Summary of model service api request duration in milliseconds  with  0.5-quantile, 0.9-quantile, 0.99-quantile | handler: ModelService        |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | service_id                   |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | party_id                     |
+                                             +         +                                                                                                                +------------------------------+
|                                             |         |                                                                                                                | action                       |
+---------------------------------------------+---------+----------------------------------------------------------------------------------------------------------------+------------------------------+

Trace
======

Secretflow-Serving supports `B3 Multiple Headers <https://github.com/openzipkin/b3-propagation?tab=readme-ov-file#multiple-headers>`_ trace propagation protocol and
uses the `Opentelemetry <https://opentelemetry.io/>`_ standard to generate trace info.
By default, trace information will not be exported, but Secretflow-Serving will still propagate trace information.
If you want to export trace information, you can specifying ``trace_config_file`` option when starting SecretFlow-Serving.
**However, exporting trace information may cause some performance degradation.**

Configure
----------

When starting SecretFlow-Serving, you can specify a config file according to :ref:`TraceConfig` in json format like this:

.. code-block:: shell

    secretflow_serving --trace_log_config=trace_config.config ...

You can also view the :ref:`example <trace_config_file>`.

Currently SecretFlow-Serving supports export span info to a trace log file.
You can specify a filename and other options in configuration.

Format
-------

Every line of the trace log file is a `ResourceSpans <https://github.com/open-telemetry/opentelemetry-proto/blob/342e1d4c3a1fe43312823ffb53bd38327f263059/opentelemetry/proto/trace/v1/trace.proto#L48>`_ in json format.

.. code-block:: json

    {
      "resource": {
        "attributes": [
          { "key": "telemetry.sdk.version", "value": { "stringValue": "1.14.2" } },
          {
            "key": "telemetry.sdk.name",
            "value": { "stringValue": "opentelemetry" }
          },
          { "key": "telemetry.sdk.language", "value": { "stringValue": "cpp" } },
          {
            "key": "service.version",
            "value": { "stringValue": "SF_SERVING_VERSION" }
          },
          {
            "key": "service.name",
            "value": { "stringValue": "Secretflow Serving" }
          }
        ]
      },
      "scopeSpans": [
        {
          "scope": {
            "name": "secretflow_serving",
            "version": "SF_SERVING_VERSION"
          },
          "spans": [
            {
              "traceId": "AABGOsNcn2QTrUhIWjlTuw==",
              "spanId": "4PWne2b7rlU=",
              "parentSpanId": "ovtKHRqW0xI=",
              "name": "PredictionService/Predict",
              "kind": "SPAN_KIND_SERVER",
              "startTimeUnixNano": "1713441077927550883",
              "endTimeUnixNano": "1713441077940197957",
              "attributes": [
                {
                  "key": "span_info",
                  "value": {
                    "stringValue": "{\"modelServiceId\":\"test_service_id\",\"partyId\":\"alice\"}"
                  }
                },
                {
                  "key": "http.request.method",
                  "value": { "stringValue": "POST" }
                },
                {
                  "key": "rpc.method",
                  "value": {
                    "stringValue": "secretflow.serving.apis.PredictionService.Predict"
                  }
                },
                {
                  "key": "rpc.service",
                  "value": {
                    "stringValue": "secretflow.serving.apis.PredictionService"
                  }
                },
                { "key": "rpc.system", "value": { "stringValue": "brpc" } },
                {
                  "key": "server.address",
                  "value": { "stringValue": "127.0.0.1:9010" }
                },
                { "key": "server.port", "value": { "intValue": "9010" } },
                {
                  "key": "source.address",
                  "value": { "stringValue": "127.0.0.1:45470" }
                },
                { "key": "source.port", "value": { "intValue": "45470" } },
                {
                  "key": "url.full",
                  "value": {
                    "stringValue": "http://127.0.0.1:9010/PredictionService/Predict"
                  }
                },
                {
                  "key": "url.path",
                  "value": { "stringValue": "/PredictionService/Predict" }
                },
                { "key": "request_protocol", "value": { "stringValue": "http" } }
              ],
              "status": { "code": "STATUS_CODE_OK" },
              "flags": 1
            }
          ]
        }
      ]
    }

Since ``TraceId`` and ``SpanId`` are defined as `bytes <https://protobuf.dev/programming-guides/proto3/#scalar>`_ in `proto file <https://github.com/open-telemetry/opentelemetry-proto/blob/342e1d4c3a1fe43312823ffb53bd38327f263059/opentelemetry/proto/trace/v1/trace.proto>`_,
so if you want to view the real value, you need to do the following conversion:

.. code-block:: python3

    import base64
    def decode_bytes(proto_bytes):
        return base64.b16encode(base64.b64decode(proto_bytes)).lower().decode()


Attributes
-----------

The spans generated by Serving contain some attributes. Some of them are filled according to
the ``opentelemetry`` standard, which has some predefined `sematic conversions <https://opentelemetry.io/docs/specs/semconv/http/http-spans/>`_.
Other Attributes such as ``span_info``, whose value is a instance of :ref:`SpanInfo` in json format,
and ``request_protocol`` represents the protocol of `brpc <https://brpc.apache.org/>`_ , you can refer to `client protocol <https://github.com/apache/brpc/blob/master/docs/en/client.md#protocols>`_
and `server protocol <https://github.com/apache/brpc/blob/master/docs/en/server.md#protocols>`_.