Introduction To Prediction Algorithms
=====================================

We will introduce two common vertical prediction models. Vertical means that the participating parties each hold different feature values of the same entity. For example, one party has height data and the other party has weight data for the same person. The two parties will use PSI or other methods to align their datas. The data they have is aligned and jointly predicted through the ID shared by both parties. This article assumes that we already have a model and does not introduce the origin of the model. If you are interested in the generation of the model, that is, the training process, you can refer to the relevant documents of `Secretflow <https://www.secretflow.org.cn/>`_ . The principle is that during the training and prediction process, both parties are invisible to each other's data, they cannot send raw data to each other.

Vertical Linear Prediction
--------------------------

Linear Models
^^^^^^^^^^^^^

The formula for the linear model is:

.. math::
    y = bias + w_1 x_1 + w_2 x_2+\dots+ w_p x_p = bias + W^TX

* :math:`x_i (i=1 - p)` represents the value of each feature.
* :math:`w_i (i = 1 - p)` represents the coefficient of each feature value.
* :math:`bias` is the bias value of the model.
* :math:`y` is the predicted value of the model.
* :math:`W^T` is the transpose of the column vector of feature coefficients.
* :math:`X` is the column vector of feature values.

That is, the predicted value of the model can be simply expressed as the dot product of the feature value and the coefficient plus the bias.
If it is a classification problem, the formula becomes:

.. math::
    y = 1 / (1 + e^{-(bias + W^TX)})

Usually we use approximate functions to describe the above functions.

Vertical Linear Model Calculation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Vertical means that multiple parties participating in the prediction hold part of the feature value. For example, assuming that the parties include Alice and Bob, predicting housing prices, Alice owns the area of the house, and Bob owns the which floor where the house is located. Then when the model is trained, Alice You will get the model coefficient of the area, Bob will get the coefficient of floor number, and the bias value will be saved in either side.

When calculating, as shown in the figure below, each party will use their own coefficients to do dot multiplication calculation (DotProduct operator), and then send it to one party for merging (MergeY operator), and finally get the result:

.. image:: /imgs/vertical_linear_calc.png
    :alt: Vertical Linear Model

Here, for the simplicity of the illustration, the area number and floor number are sent to Alice. This is not allowed because Alice shall not know Bob's feature data. When predicting, all Alice sends to Bob is the query data, and Bob will use the query data to know what data should be used for joint calculation.

Vertical Tree Model Prediction
------------------------------

Tree Model
^^^^^^^^^^

Still using the above example of house price prediction, a simple tree model can be represented by the following figure. Non-leaf nodes contain a feature value, and leaf nodes contain predicted values. When predicting, starting from root node, if the current feature value is greater than the feature value of the node, the right node of the current node is used for calculation, otherwise the left node is used. If a leaf node is reached, the value of the current leaf node is used as the predicted value. As shown in the tree below, if we predict the house price with an area of 110 and a floor of 8, the yellow node in the picture below will be used, and the final predicted value is 2:

.. image:: /imgs/tree_model.png
    :alt: Tree Model

As can be seen from the above figure, a simple tree model is relatively rough, so there is a method of using multiple trees to calculate predicted values, and XGB is one of the better models.

Vertical Tree Model
^^^^^^^^^^^^^^^^^^^

The XGB method uses multiple trees to make predictions, and the final prediction value is the sum of the values predicted by each tree (the resulting leaf node values). During XGB training, a greedy method is used to reduce the calculation amount of selecting the feature value of each node, and when training each tree, the previously trained tree will be used, that is, the residual is used for fitting. For prediction, we just need to use these already trained trees.

SGB is XGB in a vertical scenario, that is, multiple parties hold different features and jointly participate in the calculation, and during the training and prediction process, the data of each party will not be leaked.

Vertical Tree Model Prediction Calculation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For simplicity, assume that the model is just one tree, because the process for prediction is the same for each tree. The participants are Alice and Bob. Alice holds the area data and Bob holds the floor data. During training, the party holding the training data housing prices will hold the weights of all leaf nodes. That is, each holds a part of the tree, as shown below:

.. image:: /imgs/sgb_tree.png
    :alt: Vertical Trees

When predicting, there will be three operators involved. The first operator TREE_SELECT and TREE_SELECT need to be executed by all parties. The result of the execution is the label array of the leaf node. For each sample, start from the root node of the sub-model and look downwards for the leaf nodes. The search method is: if the feature value of the current node belongs to the participant himself, then select its left child node or right child node according to the the feature value; if the feature value of the current node does not belong to itself, then its left child node and the right child node will be selected, and finally a selection list of all leaf nodes will be obtained. The value of the selected leaf node is 1, otherwise it is 0.
The TREE_MERGE operator is executed on the party with the weight of the leaf node. Each party sends the selection list obtained by its TREE_SELECT to this party. It will perform an AND operation on all the selection lists. The leaf node with the result of 1 is unique, then the leaf node's weight is the result of the operator.
Each tree corresponds to a TREE_SELECT operator and a TREE_MERGE operator.
The aggregation of multiple trees is completed by TREE_ENSEMBLE_PREDICT, and like the linear model, the results can also be calculated by sigmoid. The calculation method of sigmoid here is consistent with the approximation method used during training. The result of TREE_ENSEMBLE_PREDICT is the prediction result.

.. image:: /imgs/sgb_calc.jpg
    :alt: Vertical Tree Model Calculation
