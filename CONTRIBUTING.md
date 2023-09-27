# Contribution guidelines

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project.

## Style

### C++ coding style

In general, please use clang-format to format code, and follow clang-tidy tips.

Most of the code style is derived from the
[Google C++ style guidelines](https://google.github.io/styleguide/cppguide.html), except:

- Exceptions are allowed and encouraged where appropriate.
- Header guards should use `#pragma once`.
- Adopt [camelBack](https://llvm.org/docs/Proposals/VariableNames.html#variable-names-coding-standard-options)
    for function names.

### Other tips

- Git commit message should be meaningful, we suggest imperative [keywords](https://github.com/joelparkerhenderson/git_commit_message#summary-keywords).
- Developer must write unit-test (line coverage must be greater than 80%), tests should be deterministic.
- Read awesome [Abseil Tips](https://abseil.io/tips/)

## Build

### Prerequisite


#### Docker

```sh
## start container
docker run -d -it --name serving-dev-$(whoami) \
         --mount type=bind,source="$(pwd)",target=/home/admin/dev/ \
         -w /home/admin/dev \
         --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
         --cap-add=NET_ADMIN \
         --privileged=true \
         secretflow/ubuntu-base-ci:latest

# attach to build container
docker exec -it serving-dev-$(whoami) bash
```

#### Linux

```sh
Install gcc>=11.2, cmake>=3.18, ninja, nasm>=2.15, python>=3.8, bazel==6.2.1
```

### Build & UnitTest




``` sh
# build as debug
bazel build //... -c dbg

# build as release
bazel build //... -c opt

# test
bazel test //...

# [optional] build & test with ASAN or UBSAN
bazel test //... --config=asan
bazel test //... --config=ubsan
```

### Bazel build options

- `--define gperf=on` enable gperf

## Release cycle

Secretflow-Serving recommends users "live-at-head" like [abseil-cpp](https://github.com/abseil/abseil-cpp),
just like abseil, secretflow-serving also provide Long Term Support Releases to which we backport fixes for severe bugs.

We use the release date as the version number, see [change log](CHANGELOG.md) for example.

## Change log

Please keep updating changes to the staging area of [change log](CHANGELOG.md)
Changelog should contain:

- all public API changes, including new features, breaking changes and deprecations.
- notable internal changes, like performance improvement.
