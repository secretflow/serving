# CHANGELOGS

> Instrument:
>
> - Add `[Feature]` prefix for new features
> - Add `[Bugfix]` prefix for bug fixes
> - Add `[API]` prefix for API changes
> - Add `[DOC]` prefix for Doc changes

## staging

> please add your unreleased change here.

## 20240820 - 0.6.0b0

- [Feature] enhanced Retry Strategy
- [Feature] support Kuscia Dataproxy model source
- [Infra] refator the communication logic between parties.

## 20240716 - 0.5.0b0

- [DOC] add docs for secretflow_serving_lib

## 20240625 - 0.4.0b0

- [Feature] `MERGE_Y` operator support `EXP_TAYLOR` link function
- [Feature] add graph_builder function to secretflow_serving_lib
- [Feature] handle 30x http code for model pulling
- [Feature] support `cast` function of arrow in preprocessing operator

## 20240524 - 0.3.1b0

- [Bugfix] fix tree predict base score miss
- [Bugfix] fix http adapater error msg format failed

## 20240423 - 0.3.0b0

- [Feature] Add Trace function
- [Feature] Support http model source
- [Deprecated] Python 3.8 is no longer suppoted for secretflow-serving-lib
- [Deprecated] macOS 11.x is no longer supported for secretflow-serving-lib

## 20240325 - 0.2.1b0

- [Bugfix] Fix dummy node check output schema failed

## 20240315 - 0.2.0b0

- [Feature] Add `ARROW_PROCESSING` operator
- [Feature] Add Python binding libs for serving
- [Feature] Add thread pool for ops executing
- [Feature] Node could have multiple out edges
- [Feature] Add support for Execution with specific party
- [Feature] Add `variable_inputs` tag for `OpDef`
- [Feature] Add `TREE_SELECTS`, `TREE_MERGE` and `TREE_ENSEMBLE_PREDICT` operators

## 20230928

- [serving] Init release.
