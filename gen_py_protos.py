# Copyright 2025 Ant Group Co., Ltd.
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

import os
import subprocess
import requests
import zipfile
import platform


def download_and_extract_protoc(url, extract_to):
    protoc_path = os.path.join(extract_to, 'bin', 'protoc')
    if os.path.exists(protoc_path):
        return protoc_path

    print(f"download from {url} begin.")

    zip_path = os.path.join(extract_to, 'protoc.zip')
    with requests.get(url, stream=True) as r:
        if r.status_code == 200:
            with open(zip_path, 'wb') as f:
                for chunk in r.iter_content(chunk_size=8192):
                    f.write(chunk)
        else:
            raise Exception(f"Failed to download {url}")

    print(f"download from {url} end.")

    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(extract_to)

    os.remove(zip_path)

    protoc_path = os.path.join(extract_to, 'bin', 'protoc')
    os.chmod(protoc_path, 0o755)
    return protoc_path


def generate_proto_files(protoc_path, proto_path, target_path, output_path):
    proto_files = []
    if os.path.isfile(target_path):
        proto_files.append(target_path)
    else:
        print(f"finding proto files in {target_path}")
        for root, dirs, files in os.walk(target_path):
            for file in files:
                if file.endswith('.proto'):
                    proto_files.append(os.path.join(target_path, file))

    subprocess.run(
        [protoc_path, f'--proto_path={proto_path}', f'--python_out={output_path}']
        + proto_files,
        check=True,
    )


def replace_import_statements(output_path, old_import, new_import):
    for root, dirs, files in os.walk(output_path):
        for file in files:
            if file.endswith('.py'):
                file_path = os.path.join(root, file)
                os.chmod(file_path, 0o755)
                with open(file_path, 'r', encoding='utf-8') as f:
                    content = f.read()

                content = content.replace(old_import, new_import)

                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)


def main():
    project_root = os.path.dirname(__file__)
    protos_path = os.path.join(project_root, 'secretflow_serving/protos')
    config_path = os.path.join(project_root, 'secretflow_serving/config')
    inferencer_conf_path = os.path.join(
        project_root, 'secretflow_serving/tools/inferencer/config.proto'
    )

    output_path = os.path.join(project_root, 'secretflow_serving_lib')

    old_import = 'from secretflow_serving.'
    new_import = 'from secretflow_serving_lib.secretflow_serving.'

    extract_to = os.path.join(project_root, 'protoc_temp')
    os.makedirs(extract_to, exist_ok=True)

    url = (
        'https://github.com/protocolbuffers/protobuf/releases/download/v25.6/protoc-25.6-linux-x86_32.zip',
    )
    if platform.system() == "Darwin":
        url = 'https://github.com/protocolbuffers/protobuf/releases/download/v25.6/protoc-25.6-osx-aarch_64.zip'
    else:
        if platform.machine() == "arm64" or platform.machine() == "aarch64":
            url = 'https://github.com/protocolbuffers/protobuf/releases/download/v25.6/protoc-25.6-linux-aarch_64.zip'

    protoc_path = download_and_extract_protoc(url, extract_to)

    generate_proto_files(protoc_path, project_root, protos_path, output_path)
    generate_proto_files(protoc_path, project_root, config_path, output_path)
    generate_proto_files(protoc_path, project_root, inferencer_conf_path, output_path)

    replace_import_statements(
        os.path.join(output_path, "secretflow_serving"),
        old_import,
        new_import,
    )

    print("Generation and modification complete.")


if __name__ == '__main__':
    main()
