# SecretFlow-Serving

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/secretflow/serving/tree/main.svg?style=svg)](https://dl.circleci.com/status-badge/redirect/gh/secretflow/serving/tree/main)

SecretFlow-Serving is a serving system for  privacy-preserving machine learning models.

## Serve a model

```bash
# Download the Secretflow Serving Docker image
docker pull secretflow/serving-anolis8:latest

# Start Secretflow Serving container and open the REST API port
# run alice
docker run -t --rm --name serving-example-alice --network=host \
    --entrypoint "/root/sf_serving/secretflow_serving" \
    secretflow/serving-anolis8:latest \
    "--serving_config_file=/root/sf_serving/examples/alice/serving.config" &

# run bob
docker run -t --rm --name serving-example-bob --network=host \
    --entrypoint "/root/sf_serving/secretflow_serving" \
    secretflow/serving-anolis8:latest \
    "--serving_config_file=/root/sf_serving/examples/bob/serving.config" &

# Query the model using the predict API
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

```

## Contribution Guidelines

If you would like to contribute to SecretFlow-Serving, please check [Contribution guidelines](CONTRIBUTING.md).

This documentation also contains instructions for [build and testing](CONTRIBUTING.md#build).
