.. _intro-graph:

Introduction to Graph
=====================

Secretflow-Serving has defined a protocol for describing prediction computations, which mainly includes descriptions of operators, attributes, nodes, graphs, and executions.

.. image:: /imgs/graph.png
    :alt: graph structure

Operators
---------
Operators describe specific computations. By combining operators, different model computations can be achieved. Plain operators perform computations using local data only, while secure computation operators collaborate with peer operators from other participants for secure computations.

OpDef
^^^^^

* name: Unique name of the operator.
* desc: Description of the operator.
* version: The version of the operator.
* tag: Some properties of the operator.
* attributes: Please check ``Attributes`` part below.
* inputs and output: The info of the inputs or output of the operator.

Attributes
----------
Operators have various attributes determined by their definitions. These attributes and their data support the operators in completing computations.

AttrDef
^^^^^^^

* name: Must be unique among all attrs of the operator.
* desc: Description of the attribute.
* type: Please check :ref:`AttrType <AttrType>`.
* is_optional: If True, when AttrValue is not provided, ``default_value`` would be used. Else, AttrValue must be provided.
* default_value: Please check :ref:`AttrValue <AttrValue>`.

Nodes
-----
Nodes are instances of operators. They store the attribute values (``AttrValue``) of the operators.

NodeDef
^^^^^^^

* name: Must be unique among all nodes of the graph.
* op: The operator name.
* parents: The parent node names of the node. The order of the parent nodes should match the order of the inputs of the node.
* attr_values: The attribute values config in the node. Note that this should include all attrs defined in the corresponding OpDef
* op_version: The operator version.

Graphs
------
Graphs can consist of one or multiple nodes. They form a directed acyclic graph, where the direction represents the flow of data computation. A graph can represent a complete prediction computation process, including preprocessing, model prediction, and post-processing.

Each participant will have a graph with the same structure but different data。

GraphDef
^^^^^^^^

* version: Version of the graph.
* node_list: The node list of the graph.
* execution_list: Please check ``Executions`` part below.

Executions
----------
Execution contain a subset of nodes from the main graph and form a subgraph. They represent the model computation scheduling patterns. A graph can have multiple executions.

.. image:: /imgs/execution.png
    :alt: execution

ExecutionDef
^^^^^^^^^^^^

* nodes: Represents the nodes contained in this execution. Note that these node names should be findable and unique within the node definitions. One node can only exist in one execution and must exist in one.
* config: The runtime config of the execution. It describes the scheduling logic and session-related states of this execution unit. for more details, please check :ref:`RuntimeConfig <RuntimeConfig>`.

Secretflow Serving Library
^^^^^^^^^^^^^^^^^^^^^^^^^^

Since Secretflow-Serving Load models trained by `Secretflow <https://github.com/secretflow/secretflow>`_,
Secretflow-Serving needs to export its :doc:`operators </topics/graph/operator_list>`. So
`secretflow-serving-lib <https://pypi.org/project/secretflow-serving-lib/>`_ is a python library that
provides interfaces to obtain Secretflow-Serving operators and export model files that Secretflow-Serving can load.
For more details, please check :doc:`secretflow-serving-lib docs </reference/modules>`.
