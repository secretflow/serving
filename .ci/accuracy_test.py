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
import platform
import stat
import sys
import tempfile
import time
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict, List

from pyarrow import csv as pa_csv
from google.protobuf.json_format import MessageToJson

from secretflow_serving_lib.feature_pb2 import FeatureParam
from test_common import (
    global_ip_config,
    ProcRunGuard,
    build_predict_cmd,
    ServerBaseConifg,
)


TEST_STORAGE_ROOT = os.path.join(tempfile.gettempdir(), getpass.getuser())

# set up global env
g_script_name = os.path.abspath(sys.argv[0])
g_script_dir = os.path.dirname(g_script_name)
g_repo_dir = os.path.dirname(g_script_dir)

g_clean_up_files = True


def dump_json(obj, filename, indent=2):
    with open(filename, "w") as ofile:
        json.dump(obj, ofile, indent=indent)


def is_approximately_equal(a, b, epsilon=0.0001) -> bool:
    return abs(a - b) < epsilon


class MinioTester(ProcRunGuard):
    def __init__(self, parties: List[str]):
        super().__init__()

        minio_path = tempfile.gettempdir()
        os.makedirs(minio_path, exist_ok=True)

        self.minio_server = os.path.join(minio_path, "minio")
        self.minio_client = os.path.join(minio_path, "mc")

        system = "linux"
        arch = "amd64"
        if platform.system() == "Darwin":
            system = "darwin"
        if platform.machine() == "arm64" or platform.machine() == "aarch64":
            arch = "arm64"

        print(f"get minio server")
        urllib.request.urlretrieve(
            f"https://dl.min.io/server/minio/release/{system}-{arch}/minio",
            self.minio_server,
        )
        st = os.stat(self.minio_server)
        os.chmod(self.minio_server, st.st_mode | stat.S_IEXEC)

        print(f"get minio client")
        urllib.request.urlretrieve(
            f"https://dl.min.io/client/mc/release/{system}-{arch}/mc",
            self.minio_client,
        )
        st = os.stat(self.minio_client)
        os.chmod(self.minio_client, st.st_mode | stat.S_IEXEC)

        minio_data_path = os.path.join(minio_path, "data")

        ms_env = os.environ.copy()
        ms_env["MINIO_BROWSER"] = "off"
        ms_env["MINIO_ACCESS_KEY"] = "serving_test_aaa"
        ms_env["MINIO_SECRET_KEY"] = "serving_test_sss"

        self.s3_configs = {}
        endpoint = "127.0.0.1:63111"
        for p in parties:
            config = {
                "access_key": ms_env['MINIO_ACCESS_KEY'],
                "secret_key": ms_env['MINIO_SECRET_KEY'],
                "virtual_hosted": False,
                "endpoint": endpoint,
                "bucket": p,
            }
            os.makedirs(os.path.join(minio_data_path, config["bucket"]), exist_ok=True)
            self.s3_configs[p] = config

        # start minio server
        self.run_cmd(
            f"{self.minio_server} server {minio_data_path} --address {endpoint}",
            True,
            ms_env,
        )
        # wait server start
        time.sleep(10)

        # config mc client
        self.run_cmd(
            f"{self.minio_client} alias set 'serving_test' 'http://{endpoint}' \'{ms_env['MINIO_ACCESS_KEY']}\' \'{ms_env['MINIO_SECRET_KEY']}\'"
        )

    def put_file(self, file_path: str, party: str):
        put_model_cmd = f"{self.minio_client} put {file_path} serving_test/{self.s3_configs[party]['bucket']}/{os.path.basename(file_path)}"
        self.run_cmd(put_model_cmd)

    def get_s3_configs(self) -> Dict[str, dict]:
        return self.s3_configs


@dataclass
class PartyConfig(ServerBaseConifg):
    model_package_path: str = None
    csv_path: str = None
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

    def _dump_serving_config(
        self,
        path: str,
        config: PartyConfig,
        use_oss: bool = False,
        party_s3_configs: Dict[str, dict] = None,
    ):
        model_config_dict = {
            "modelId": config.model_id,
            "basePath": os.path.abspath(path),
            "sourcePath": os.path.abspath(config.model_package_path),
            "sourceType": "ST_FILE",
        }
        if use_oss:
            assert party_s3_configs
            model_config_dict['sourceType'] = "ST_OSS"
            model_config_dict['sourcePath'] = os.path.basename(
                config.model_package_path
            )
            model_config_dict['ossSourceMeta'] = party_s3_configs[config.id]

        config_dict = {
            "id": self.service_id,
            "serverConf": {
                "host": config.host,
                "servicePort": config.service_port,
                "communicationPort": config.communication_port,
                "metricsExposerPort": config.metrics_port,
                "brpcBuiltinServicePort": config.brpc_builtin_service_port,
            },
            "modelConf": model_config_dict,
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

    def finish(
        self,
        path=".",
        use_oss: bool = False,
        party_s3_configs: Dict[str, dict] = None,
    ):
        for id, config in self.party_configs.items():
            config_path = os.path.join(path, id)
            if not os.path.exists(config_path):
                os.makedirs(config_path, exist_ok=True)
            self.logging_config_paths[id] = self._dump_logging_config(
                config_path, os.path.join(config_path, "log")
            )
            self.serving_config_paths[id] = self._dump_serving_config(
                config_path, config, use_oss, party_s3_configs
            )

    def get_logging_config_paths(self) -> Dict[str, str]:
        return self.logging_config_paths

    def get_serving_config_paths(self) -> Dict[str, str]:
        return self.serving_config_paths


# simple test
class AccuracyTestCase(ProcRunGuard):
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
        use_oss: bool = False,
    ):
        super().__init__()
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
        self.use_oss = False
        if use_oss:
            self.use_oss = use_oss
            self.minio_tester = MinioTester(parties)

    def start_server(self, start_interval_s=0):
        if self.use_oss:
            # put model file
            for p, config in self.party_configs.items():
                self.minio_tester.put_file(config.model_package_path, p)

        config_builder = ConfigBuilder(
            party_configs=self.party_configs,
            service_id=self.service_id,
        )
        config_builder.finish(
            self.data_path,
            self.use_oss,
            None if not self.use_oss else self.minio_tester.get_s3_configs(),
        )

        logging_config_paths = config_builder.get_logging_config_paths()
        serving_config_paths = config_builder.get_serving_config_paths()

        for id in self.party_configs.keys():
            self.run_cmd(
                f"./bazel-bin/secretflow_serving/server/secretflow_serving --serving_config_file={serving_config_paths[id]} --logging_config_file={logging_config_paths[id]}",
                True,
            )
            if start_interval_s:
                time.sleep(start_interval_s)

        # wait 10s for servers be ready
        time.sleep(10)

    def stop_server(self):
        self.cleanup_sub_procs()
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

    def exec(self, epsilon=0.0001):
        try:
            self.start_server()

            # read expect csv
            expect_table = pa_csv.read_csv(self.expect_csv_path)
            expect_df = expect_table.to_pandas()

            print(f"expect_df: {expect_df}")
            for party, config in self.party_configs.items():
                res = self.run_cmd(
                    build_predict_cmd(
                        config.host, config.service_port, self._make_request_body()
                    )
                )
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
                        expect_score, s, epsilon
                    ), f'result not match, {s} vs {expect_score}'
        finally:
            self.stop_server()


if __name__ == "__main__":
    AccuracyTestCase(
        service_id="glm",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/glm',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred_y',
    ).exec(0.1)

    AccuracyTestCase(
        service_id="sgb",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/sgb',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '11', '17', '18'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="sgb_fetures_in_one_party",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/fetures_in_one_party/sgb',
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
        service_id="xgb_fetures_in_one_party",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/fetures_in_one_party/xgb',
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
        service_id="sgd_fetures_in_one_party",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/fetures_in_one_party/sgd',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="ou_sgd",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/ou_sgd',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="ou_sgd_fetures_in_one_party",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/fetures_in_one_party/ou_sgd',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred',
    ).exec()

    AccuracyTestCase(
        service_id="ou_glm",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/ou_glm',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred_y',
    ).exec(0.1)

    AccuracyTestCase(
        service_id="ou_xgb",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/ou_xgb',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred_y',
    ).exec()

    AccuracyTestCase(
        service_id="ou_xgb_features_in_one_party",
        parties=['alice', 'bob'],
        case_dir='.ci/test_data/fetures_in_one_party/ou_xgb',
        package_name='s_model.tar.gz',
        input_csv_names={'alice': 'alice.csv', 'bob': 'bob.csv'},
        expect_csv_name='predict.csv',
        query_ids=['1', '2', '3', '4', '5', '6', '7', '8', '9', '15'],
        score_col_name='pred_y',
    ).exec()
