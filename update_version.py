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
import re
import datetime
import sys


def update_version():
    BASE_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))

    version_file = os.path.join(BASE_DIR, 'version.bzl')
    with open(version_file, 'r') as f:
        lines = f.readlines()

    version_pattern = re.compile(r'^\s*(SERVING_VERSION\s*=\s*)"([^"]+)"', re.MULTILINE)
    version_key = None
    version_value = None
    version_line_idx = -1
    for i, line in enumerate(lines):
        match = version_pattern.search(line)
        if match:
            version_key = match.group(1).strip()
            version_value = match.group(2)
            version_line_idx = i
            break

    assert version_line_idx > 0

    new_version = version_value

    # update version
    if version_value and version_value.endswith('dev'):
        date_str = datetime.datetime.now().strftime('%Y%m%d')
        new_version = version_value + date_str

        lines[version_line_idx] = f'{version_key} "{new_version}"\n'

        with open(version_file, 'w') as f:
            f.writelines(lines)

    print(f"binary version: {new_version}")

    version_h_path = os.path.join(BASE_DIR, 'secretflow_serving/server/version.h')
    if os.path.exists(version_h_path):
        with open(version_h_path, 'r') as f:
            content = f.read()

        new_content = content.replace('SF_SERVING_VERSION', str(new_version))

        with open(version_h_path, 'w') as f:
            f.write(new_content)
    else:
        print(f"version.h not found at: {version_h_path}")


if __name__ == '__main__':
    update_version()
