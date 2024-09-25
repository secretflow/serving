# Usage

## C++

### Alice

```bash
bazel-bin/secretflow_serving/tools/inferencer/inferencer --serving_config_file=secretflow_serving/tools/inferencer/example/alice/serving.config --inference_config_file=secretflow_serving/tools/inferencer/example/alice/inference.config
```

### Bob

```bash
bazel-bin/secretflow_serving/tools/inferencer/inferencer --serving_config_file=secretflow_serving/tools/inferencer/example/bob/serving.config --inference_config_file=secretflow_serving/tools/inferencer/example/bob/inference.config
```

## Python

```python
  import importlib

  with importlib.resources.path('secretflow_serving.tools.inferencer', 'inferencer') as tool_path:

  # dump serving config file

  # dump inference config file

  # run inferencer...
```
