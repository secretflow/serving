#! python3

# Copyright 2023 Ant Group Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import getpass
import json
import os
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from typing import Any, Dict, List

from pyarrow import csv as pa_csv
from google.protobuf.json_format import MessageToJson

from secretflow_serving_lib.feature_pb2 import FeatureParam
from test_common import global_ip_config


TEST_STORAGE_ROOT = os.path.join(tempfile.gettempdir(), getpass.getuser())

# set up global env
g_script_name = os.path.abspath(sys.argv[0])
g_script_dir = os.path.dirname(g_script_name)
g_repo_dir = os.path.dirname(g_script_dir)

g_clean_up_service = True
g_clean_up_files = True


def dump_json(obj, filename, indent=2):
    with open(filename, "w") as ofile:
        json.dump(obj, ofile, indent=indent)


def is_approximately_equal(a, b, epsilon) -> bool:
    return abs(a - b) < epsilon


class CurlWrapper:
    def __init__(self, url: str, data: str):
        self.url = url
        self.data = data

    def cmd(self):
        return f'curl --location "{self.url}" --header "Content-Type: application/json" --data \'{self.data}\''

    def exe(self):
        return os.popen(self.cmd())


@dataclass
class PartyConfig:
    id: str
    cluster_ip: str
    host: str
    service_port: int
    communication_port: int
    metrics_port: int
    brpc_builtin_service_port: int
    channel_protocol: str
    model_id: str
    model_package_path: str
    csv_path: str
    query_datas: List[str] = None
    query_context: str = None


class ConfigBuilder:
    def __init__(
        self,
        party_configs: Dict[str, PartyConfig],
        service_id: str,
        serving_config_filename: str = "serving.config",
        log_config_filename: str = "logging.config",
    ):
        self.service_id = service_id
        self.party_configs = party_configs
        self.parties = []
        self.log_config = log_config_filename
        self.serving_config = serving_config_filename
        for id, config in self.party_configs.items():
            self.parties.append({"id": id, "address": config.cluster_ip})

        self.logging_config_paths = {}
        self.serving_config_paths = {}

    def _dump_logging_config(self, path: str, log_path: str):
        logging_config_path = os.path.join(path, self.log_config)
        with open(logging_config_path, "w") as ofile:
            json.dump({"systemLogPath": os.path.abspath(log_path)}, ofile, indent=2)
        return logging_config_path

    def _dump_serving_config(self, path: str, config: PartyConfig):
        config_dict = {
            "id": self.service_id,
            "serverConf": {
                "host": config.host,
                "servicePort": config.service_port,
                "communicationPort": config.communication_port,
                "metricsExposerPort": config.metrics_port,
                "brpcBuiltinServicePort": config.brpc_builtin_service_port,
            },
            "modelConf": {
                "modelId": config.model_id,
                "basePath": os.path.abspath(path),
                "sourcePath": os.path.abspath(config.model_package_path),
                "sourceType": "ST_FILE",
            },
            "clusterConf": {
                "selfId": config.id,
                "parties": self.parties,
                "channelDesc": {"protocol": config.channel_protocol},
            },
            "featureSourceConf": {
                "csvOpts": {
                    "filePath": os.path.abspath(config.csv_path),
                    "id_name": "id",
                }
            },
        }
        config_path = os.path.join(path, self.serving_config)
        dump_json(config_dict, config_path)
        return config_path

    def finish(self, path="."):
        for id, config in self.party_configs.items():
            config_path = os.path.join(path, id)
            if not os.path.exists(config_path):
                os.makedirs(config_path, exist_ok=True)
            self.logging_config_paths[id] = self._dump_logging_config(
                config_path, os.path.join(config_path, "log")
            )
            self.serving_config_paths[id] = self._dump_serving_config(
                config_path, config
            )

    def get_logging_config_paths(self) -> Dict[str, str]:
        return self.logging_config_paths

    def get_serving_config_paths(self) -> Dict[str, str]:
        return self.serving_config_paths


# simple test
class AccuracyTestCase:
    def __init__(
        self,
        service_id: str,
        parties: List[str],
        case_dir: str,
        package_name: str,
        input_csv_names: Dict[str, str],
        expect_csv_name: str,
        query_ids: List[str],
        score_col_name: str,
    ):
        self.service_id = service_id
        self.case_dir = case_dir
        self.expect_csv_path = os.path.join(case_dir, expect_csv_name)
        self.query_ids = query_ids
        self.score_col_name = score_col_name
        self.party_configs = {}
        for index, party in enumerate(parties):
            base_dir = os.path.join(case_dir, party)
            self.party_configs[party] = PartyConfig(
                id=party,
                **global_ip_config(index),
                channel_protocol="baidu_std",
                model_id="accuracy_test",
                model_package_path=os.path.join(base_dir, package_name),
                csv_path=os.path.join(base_dir, input_csv_names[party]),
                query_datas=query_ids,
            )
        self.background_proc = []
        self.data_path = os.path.join(TEST_STORAGE_ROOT, self.service_id)

    def _exe_cmd(self, cmd, background=False):
        print("Execute: ", cmd)
        if not background:
            ret = subprocess.run(cmd, shell=True, check=True, capture_output=True)
            ret.check_returncode()
            return ret
        else:
            proc = subprocess.Popen(cmd.split(), shell=False)
            self.background_proc.append(proc)
            return proc

    def start_server(self, start_interval_s=0):
        config_builder = ConfigBuilder(
            party_configs=self.party_configs, service_id=self.service_id
        )
        config_builder.finish(self.data_path)

        logging_config_paths = config_builder.get_logging_config_paths()
        serving_config_paths = config_builder.get_serving_config_paths()

        for id in self.party_configs.keys():
            self._exe_cmd(
                f"./bazel-bin/secretflow_serving/server/secretflow_serving --serving_config_file={serving_config_paths[id]} --logging_config_file={logging_config_paths[id]}",
                True,
            )
            if start_interval_s:
                time.sleep(start_interval_s)

        # wait 10s for servers be ready
        time.sleep(10)

    def stop_server(self):
        if g_clean_up_service:
            for proc in self.background_proc:
                proc.kill()
                proc.wait()
        if g_clean_up_files:
            os.system(f"rm -rf {self.data_path}")

    def _make_request_body(self):
        fs_param = {}
        for id, config in self.party_configs.items():
            fs_param[id] = FeatureParam(query_datas=self.query_ids)

        body_dict = {
            "service_spec": {
                "id": self.service_id,
            },
            "fs_params": {
                k: json.loads(MessageToJson(v, preserving_proto_field_name=True))
                for k, v in fs_param.items()
            },
        }

        return json.dumps(body_dict)

    def make_predict_curl_cmd(self, party: str):
        url = None
        for id, config in self.party_configs.items():
            if id == party:
                url = f"http://{config.host}:{config.service_port}/PredictionService/Predict"
                break
        if not url:
            raise Exception(f"{party} is not in config({self.party_configs.keys()})")
        curl_wrapper = CurlWrapper(
            url=url,
            data=self._make_request_body(),
        )
        return curl_wrapper.cmd()

    def exec(self):
        try:
            self.start_server()

            # read expect csv
            expect_table = pa_csv.read_csv(self.expect_csv_path)
            expect_df = expect_table.to_pandas()

            print(f"expect_df: {expect_df}")

            for party in self.party_configs.keys():
                res = self._exe_cmd(self.make_predict_curl_cmd(party))
                out = res.stdout.decode()
                print("Predict Result: ", out)
                res = json.loads(out)
                assert (
                    res["status"]["code"] == 1
                ), f'return status code({res["status"]["code"]}) should be OK(1)'
                assert len(res['results']) == len(
                    self.query_ids
                ), f'return results size not match the query size {len(res["results"])} vs {len(self.query_ids)}'

                # check result accuracy
                for index, r in enumerate(res['results']):
                    # TODO: support multi-col score
                    s = float(r["scores"][0]["value"])
                    id = self.query_ids[index]
                    expect_score = float(
                        expect_df[expect_df['id'] == int(id)][self.score_col_name].iloc[
                            0
                        ]
                    )
                    assert is_approximately_equal(
                        expect_score, s, 0.0001
                    ), f'result not match, {s} vs {expect_score}'
        finally:
            self.stop_server()


if __name__ == "__main__":
    AccuracyTestCase(
        service_id="bin_onehot_glm",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/bin_onehot_glm',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="bin_onehot_glm_alice_no_feature",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/bin_onehot_glm_alice_no_feature',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="bin_sgb",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/bin_sgb',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '11', '17', '18'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="bin_sgb_alice_no_feature",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/bin_sgb_alice_no_feature',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '11', '17', '18'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="xgb",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/xgb',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '11', '17', '18'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="xgb_alice_no_feature",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/xgb_alice_no_feature',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '11', '17', '18'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="sgd",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/sgd',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="sgd_alice_no_feature",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/sgd_alice_no_feature',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()
