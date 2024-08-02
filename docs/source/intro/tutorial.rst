Quickstart
==========

TL;DR
-----

Use ``docker-compose`` to deploy a SecretFlow-Serving cluster, the query the model using the predict API.


Start SecretFlow-Serving Service
--------------------------------

You could start SecretFlow-Serving service via `docker-compose <https://github.com/secretflow/serving/tree/main/examples>`_, it would deploy and start services as shown in the following figure, it contains two SecretFlow-Serving from party ``Alice``, ``Bob``.

.. image:: /imgs/architecture.png
    :alt: docker-compose deployment for quickstart example


.. note::
    To demonstrate SecretFlow-Serving, we conducted the following simplified operations:

    1. Both parties of Secretflow-Serving use mock feature source to produce random feature values.
    2. The model files in the examples directory are loaded by ``Alice`` and ``Bob``'s Secretflow-Serving respectively。
    3. The SecretFlow-Serving is served through the HTTP protocol. However, for production environments, it is recommended to use HTTPS instead. Please check :ref:`TLS Configuration <TlsConfig>` for details.


.. code-block:: bash

    # startup docker-compose
    # If you install docker with Compose V1, pleas use `docker-compose` instead of `docker compose`
    (cd examples && docker compose up -d)

Now, the ``Alice``'s SecretFlow-Serving is listening on ``http://localhost:9010``, the ``Bob``'s SecretFlow-Serving is listening on ``http://localhost:9011``, you could send predict request to it via curl or other http tools.


Do Predict
----------

send predict request to ``Alice``


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

send predict request to ``Bob``

.. code-block:: bash

    curl --location 'http://127.0.0.1:9011/PredictionService/Predict' \
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

.. note::
    Please checkout :ref:`SecretFlow-Serving API <PredictionService>` for the Predict API details.
