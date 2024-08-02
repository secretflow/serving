SecretFlow-Serving System Introduction
======================================

SecretFlow-Serving is a serving system for privacy-preserving machine learning models.

Key Features
------------

* One process one model/version.
* Support multiple parties (N >= 2).
* Parallel compute between parties.
* Batch Predict API Supported.
* Multi-protocol support. Secretflow-Serving is built on `brpc <https://brpc.apache.org/>`_, a high-performance rpc framework, and is capable of using multiple communication protocols.
* Support multiple types feature sources, e.g. SPI, CSV file, Mock data.
* Specific model graph definition.


Architecture
------------

Secretflow-Serving leverages the model package trained with Secretflow to provide model prediction capabilities at different security levels. It achieves this by utilizing the online feature data provided by each participant without compromising the integrity of the original data domain.

.. image:: /imgs/architecture.png
    :alt: Secretflow-Serving Deployment Architecture


Key Concepts
^^^^^^^^^^^^

To understand the architecture of Secretflow-Serving, you need to understand the following key concepts:


Model Package
+++++++++++++

A Secretflow-Serving model package is a compressed package comprising a model file, a manifest file, and other metadata files.

The manifest file provides meta-information about the model file and follows the defined structure outlined :ref:`here <ModelManifest>`.

The model file contains the graph that represents the model inference process, encompassing pre-processing, post-processing, and the specific inference algorithm. For graph details, please check :ref:`Introduction to Graph <intro-graph>`.

The metadata files, while optional, stores additional data information required during the model inference process.


Model Source
+++++++++++++

Secretflow-Serving supports retrieving model packages from different storage sources. Currently, the following data sources are supported:

* Local Filesystem Data Source: Secretflow-Serving loads the model package from a specified local path.
* OSS/S3 Data Source: Secretflow-Serving attempts to download the model package from the OSS/S3 storage based on the provided configuration before loading it locally.


Feature Source
+++++++++++++++

Secretflow-Serving obtains the necessary features for the online inference process through the Feature Source. Currently, the platform supports the following feature data sources:

* HTTP Source: Secretflow-Serving defines a Service Provider Interface (:doc:`SPI </reference/spi>`) for retrieving feature data. Feature providers can implement this SPI to supply features to Secretflow-Serving.
* CSV Source: Secretflow-Serving supports direct loading of CSV file as a feature source. For performance reasons, the CSV file is fully loaded into memory and features are filtered based on the ID column.
* Mock Source: In this scenario, Secretflow-Serving uses randomly generated values as feature data.
