from importlib import resources
import asyncio
import os

current_file_path = os.path.abspath(__file__)
code_dir = os.path.dirname(os.path.dirname(current_file_path))

alice_serving_config_file_path = os.path.join(
    code_dir,
    "secretflow_serving/tools/inferencer/example/alice/serving.config",
)
alice_inference_config_file_path = os.path.join(
    code_dir,
    "secretflow_serving/tools/inferencer/example/alice/inference.config",
)
bob_serving_config_file_path = os.path.join(
    code_dir,
    "secretflow_serving/tools/inferencer/example/bob/serving.config",
)
bob_inference_config_file_path = os.path.join(
    code_dir,
    "secretflow_serving/tools/inferencer/example/bob/inference.config",
)


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


async def main():
    with resources.path(
        'secretflow_serving.tools.inferencer', 'inferencer'
    ) as tool_path:
        alice_command = [
            str(tool_path),
            f'--serving_config_file={alice_serving_config_file_path}',
            f'--inference_config_file={alice_inference_config_file_path}',
        ]
        bob_command = [
            str(tool_path),
            f'--serving_config_file={bob_serving_config_file_path}',
            f'--inference_config_file={bob_inference_config_file_path}',
        ]
        commands = [alice_command, bob_command]

    tasks = [run_process(command) for command in commands]

    await asyncio.gather(*tasks)


if __name__ == '__main__':
    asyncio.run(main())
