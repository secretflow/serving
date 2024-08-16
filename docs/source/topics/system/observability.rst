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
you may start metric service by specifying ``metrics_exposer_port`` of :ref:`ServerConfig`.
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
