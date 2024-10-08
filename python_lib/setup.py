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
# Ideas borrowed from: https://github.com/ray-project/ray/blob/master/python/setup.py

import io
import logging
import os
import platform
import re
import shutil
import subprocess
import sys
import setuptools
import setuptools.command.build_ext

from datetime import datetime, timedelta

logger = logging.getLogger(__name__)
# 3.9 is the minimum python version we can support
SUPPORTED_PYTHONS = [(3, 9), (3, 10), (3, 11)]
BAZEL_MAX_JOBS = os.getenv("BAZEL_MAX_JOBS")
ROOT_DIR = os.path.dirname(__file__)
SKIP_BAZEL_CLEAN = os.getenv("SKIP_BAZEL_CLEAN")
BAZEL_CACHE_DIR = os.getenv("BAZEL_CACHE_DIR")

pyd_suffix = ".so"


def add_date_to_version(version_txt_file, version_py_file):
    local_time = datetime.utcnow()
    chn_time = local_time + timedelta(hours=8)
    dstr = chn_time.strftime("%Y%m%d")
    with open(os.path.join(ROOT_DIR, version_txt_file), "r") as fp:
        content = fp.read()
    content = content.replace("$$DATE$$", dstr)

    with open(os.path.join(ROOT_DIR, version_txt_file), "w+") as fp:
        fp.write(content)

    # write to py version file
    with open(os.path.join(ROOT_DIR, version_py_file), "w+") as fp:
        fp.write(content)


def find_version(version_txt_file, version_py_file):
    add_date_to_version(version_txt_file, version_py_file)
    # Extract version information from filepath
    with open(os.path.join(ROOT_DIR, version_py_file)) as fp:
        version_match = re.search(
            r"^__version__ = ['\"]([^'\"]*)['\"]", fp.read(), re.M
        )
        if version_match:
            return version_match.group(1)
        raise RuntimeError("Unable to find version string.")


def read_requirements(*filepath):
    requirements = []
    with open(os.path.join(ROOT_DIR, *filepath)) as file:
        requirements = file.read().splitlines()
    return requirements


class SetupSpec:
    def __init__(self, name: str, description: str):
        self.name: str = name
        self.version = find_version(
            "../version.txt", "./secretflow_serving_lib/version.py"
        )
        self.description: str = description
        self.files_to_include: list = []
        self.install_requires: list = []
        self.extras: dict = {}

    def get_packages(self):
        return setuptools.find_packages("./")


setup_spec = SetupSpec(
    "secretflow-serving-lib",
    "Serving is a subproject of Secretflow that implements model serving capabilities.",
)

# These are the directories where automatically generated Python protobuf
# bindings are created.
generated_python_directories = [
    "../bazel-bin/python_lib",
    "../bazel-bin/secretflow_serving/protos",
    "../bazel-bin/secretflow_serving/config",
    "../bazel-bin/secretflow_serving/tools/inferencer",
]
setup_spec.install_requires = read_requirements("requirements.txt")
files_to_remove = []


# NOTE: The lists below must be kept in sync with secretflow_serving_lib/BUILD.bazel.
serving_ops_lib_files = [
    "../bazel-bin/python_lib/secretflow_serving_lib/libserving" + pyd_suffix
]

serving_tools_files = ["../bazel-bin/secretflow_serving/tools/inferencer/inferencer"]


# Calls Bazel in PATH
def bazel_invoke(invoker, cmdline, *args, **kwargs):
    try:
        result = invoker(["bazel"] + cmdline, *args, **kwargs)
        return result
    except IOError:
        raise


def build():
    if tuple(sys.version_info[:2]) not in SUPPORTED_PYTHONS:
        msg = (
            "Detected Python version {}, which is not supported. "
            "Only Python {} are supported."
        ).format(
            ".".join(map(str, sys.version_info[:2])),
            ", ".join(".".join(map(str, v)) for v in SUPPORTED_PYTHONS),
        )
        raise RuntimeError(msg)

    bazel_env = dict(os.environ, PYTHON3_BIN_PATH=sys.executable)

    bazel_flags = ["--verbose_failures"]
    if BAZEL_MAX_JOBS:
        n = int(BAZEL_MAX_JOBS)  # the value must be an int
        bazel_flags.append("--jobs")
        bazel_flags.append(f"{n}")
    if BAZEL_CACHE_DIR:
        bazel_flags.append(f"--repository_cache={BAZEL_CACHE_DIR}")

    bazel_precmd_flags = []

    bazel_targets = [
        "//python_lib/secretflow_serving_lib:init",
        "//python_lib/secretflow_serving_lib:api",
        "//python_lib/secretflow_serving_lib/config",
        "//secretflow_serving/tools/inferencer",
    ]

    bazel_flags.extend(["-c", "opt"])

    if platform.machine() == "x86_64":
        bazel_flags.extend(["--config=avx"])

    return bazel_invoke(
        subprocess.check_call,
        bazel_precmd_flags + ["build"] + bazel_flags + ["--"] + bazel_targets,
        env=bazel_env,
    )


def remove_prefix(text, prefix):
    return text[text.startswith(prefix) and len(prefix) :]


def copy_file(target_dir, filename, rootdir):
    source = os.path.relpath(filename, rootdir)
    if source.startswith('../bazel-bin/python_lib'):
        destination = os.path.join(
            target_dir, remove_prefix(source, "../bazel-bin/python_lib/")
        )
    else:
        destination = os.path.join(target_dir, remove_prefix(source, "../bazel-bin/"))

    # Create the target directory if it doesn't already exist.
    print(f'Create dir {os.path.dirname(destination)}')
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    if not os.path.exists(destination):
        print(f"Copy file from {source} to {destination}")
        shutil.copy(source, destination, follow_symlinks=True)
        return 1
    return 0


def remove_file(target_dir, filename):
    file = os.path.join(target_dir, filename)
    if os.path.exists(file):
        print(f"delete {file}")
        os.remove(file)
        return 1
    return 0


def pip_run(build_ext):
    build()

    setup_spec.files_to_include += serving_ops_lib_files
    setup_spec.files_to_include += serving_tools_files

    # Copy over the autogenerated protobuf Python bindings.
    for directory in generated_python_directories:
        for filename in os.listdir(directory):
            if filename[-3:] == ".py":
                setup_spec.files_to_include.append(os.path.join(directory, filename))
    print(f"generated files: {setup_spec.files_to_include}")

    print(f"generated files: {setup_spec.files_to_include}")
    copied_files = 0
    for filename in setup_spec.files_to_include:
        copied_files += copy_file(build_ext.build_lib, filename, ROOT_DIR)
    print("# of files copied to {}: {}".format(build_ext.build_lib, copied_files))

    deleted_files = 0
    for filename in files_to_remove:
        deleted_files += remove_file(build_ext.build_lib, filename)
    print("# of files deleted in {}: {}".format(build_ext.build_lib, deleted_files))


class build_ext(setuptools.command.build_ext.build_ext):
    def run(self):
        return pip_run(self)


class BinaryDistribution(setuptools.Distribution):
    def has_ext_modules(self):
        return True


# Ensure no remaining lib files.
build_dir = os.path.join(ROOT_DIR, "build")
if os.path.isdir(build_dir):
    shutil.rmtree(build_dir)

if not SKIP_BAZEL_CLEAN:
    bazel_invoke(subprocess.check_call, ["clean"])

# Default Linux platform tag
plat_name = "manylinux2014_x86_64"

if sys.platform == "darwin":
    # Due to a bug in conda x64 python, platform tag has to be 10_16 for X64 wheel
    if platform.machine() == "x86_64":
        plat_name = "macosx_10_16_x86_64"
    else:
        plat_name = "macosx_12_0_arm64"
elif platform.machine() == "aarch64":
    # Linux aarch64
    plat_name = "manylinux_2_28_aarch64"

setuptools.setup(
    name=setup_spec.name,
    version=setup_spec.version,
    author="SecretFlow Team",
    author_email="secretflow-contact@service.alipay.com",
    description=(setup_spec.description),
    long_description=io.open(
        os.path.join(ROOT_DIR, "../README.md"), "r", encoding="utf-8"
    ).read(),
    long_description_content_type="text/markdown",
    classifiers=[
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
    ],
    packages=setuptools.find_packages(exclude=("tests", "tests.*")),
    cmdclass={"build_ext": build_ext},
    # The BinaryDistribution argument triggers build_ext.
    distclass=BinaryDistribution,
    install_requires=setup_spec.install_requires,
    setup_requires=["wheel"],
    extras_require=setup_spec.extras,
    license="Apache 2.0",
    options={"bdist_wheel": {"plat_name": plat_name}},
)
