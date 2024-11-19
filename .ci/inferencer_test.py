# Copyright 2024 Ant Group Co., Ltd.
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


from importlib import resources
import asyncio
import os
import pandas as pd
import numpy as np

current_file_path = os.path.abspath(__file__)
code_dir = os.path.dirname(os.path.dirname(current_file_path))


async def run_process(command):
    process = await asyncio.create_subprocess_exec(
        *command, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
    )
    stdout, stderr = await process.communicate()
    if process.returncode == 0:
        print(
            f"Process {' '.join(command)} completed successfully:\n{stdout.decode().strip()}"
        )
    else:
        print(
            f"Process {' '.join(command)} failed with exit code {process.returncode}:\n{stderr.decode().strip()}"
        )


async def run_inferencer_example(
    exmaple_dir: str,
    result_path: str,
    target_path: str,
    target_col_name: str,
    epsilon=0.0001,
):
    print(f"====begin example: {exmaple_dir}=====")

    with resources.path(
        'secretflow_serving.tools.inferencer', 'inferencer'
    ) as tool_path:
        alice_command = [
            str(tool_path),
            f'--serving_config_file={exmaple_dir}/alice/serving.config',
            f'--inference_config_file={exmaple_dir}/alice/inference.config',
        ]
        bob_command = [
            str(tool_path),
            f'--serving_config_file={exmaple_dir}/bob/serving.config',
            f'--inference_config_file={exmaple_dir}/bob/inference.config',
        ]
        commands = [alice_command, bob_command]

    tasks = [run_process(command) for command in commands]

    await asyncio.gather(*tasks)

    result_df = pd.read_csv(result_path)
    target_df = pd.read_csv(target_path)

    score_col = result_df['score']
    pred_col = target_df[target_col_name]

    assert len(score_col) == len(pred_col)

    are_close = np.isclose(score_col, pred_col, atol=epsilon)

    for i, match in enumerate(are_close):
        assert match, f"row {i} mismatch: {score_col[i]} != {pred_col[i]}"


if __name__ == '__main__':
    asyncio.run(
        run_inferencer_example(
            "secretflow_serving/tools/inferencer/example/normal",
            "tmp/alice/score.csv",
            ".ci/test_data/glm/predict.csv",
            "pred_y",
            0.01,
        )
    )

    asyncio.run(
        run_inferencer_example(
            "secretflow_serving/tools/inferencer/example/one_party_no_feature",
            "tmp/bob/score.csv",
            ".ci/test_data/fetures_in_one_party/sgd/predict.csv",
            "pred",
        )
    )
