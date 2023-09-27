// Copyright 2023 Ant Group Co., Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "secretflow_serving/util/sys_util.h"

#include <errno.h>
#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/escaping.h"
#include "openssl/md5.h"
#include "spdlog/spdlog.h"

#include "secretflow_serving/core/exception.h"

namespace secretflow::serving {

namespace {

int cmd_through_popen(std::ostream& os, const char* cmd) {
  FILE* pipe = popen(cmd, "r");
  if (pipe == NULL) {
    return -1;
  }
  char buffer[1024];
  for (;;) {
    size_t nr = fread(buffer, 1, sizeof(buffer), pipe);
    if (nr != 0) {
      os.write(buffer, nr);
    }
    if (nr != sizeof(buffer)) {
      if (feof(pipe)) {
        break;
      } else if (ferror(pipe)) {
        SPDLOG_ERROR("encountered error while reading for the pipe");
        break;
      }
      // retry;
    }
  }

  const int wstatus = pclose(pipe);

  if (wstatus < 0) {
    return wstatus;
  }
  if (WIFEXITED(wstatus)) {
    return WEXITSTATUS(wstatus);
  }
  if (WIFSIGNALED(wstatus)) {
    os << "child process was killed by signal " << WTERMSIG(wstatus);
  }
  errno = ECHILD;
  return -1;
}

namespace {

std::string MD5String(const std::string& str) {
  unsigned char results[MD5_DIGEST_LENGTH];
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, str.data(), str.length());
  MD5_Final(results, &ctx);
  return absl::BytesToHexString(absl::string_view(
      reinterpret_cast<const char*>(results), MD5_DIGEST_LENGTH));
}

}  // namespace

}  // namespace

void SysUtil::System(const std::string& cmd, std::string* command_output) {
  std::ostringstream cmd_output;
  int ret = cmd_through_popen(cmd_output, cmd.c_str());
  if (ret != 0) {
    std::string content = cmd_output.str();
    if (content.length() > 2048) {
      content.resize(2048);
    }
    YACL_THROW("execute cmd={} return error code={}: {}", cmd, ret, content);
  }
  if (command_output) {
    *command_output = cmd_output.str();
  }
}

void SysUtil::ExtractGzippedArchive(const std::string& package_path,
                                    const std::string& target_dir) {
  if (!std::filesystem::exists(package_path) ||
      std::filesystem::file_size(package_path) == 0) {
    YACL_THROW("file {} not exist or file size == 0. extract fail",
               package_path);
  }

  std::filesystem::create_directories(target_dir);
  auto cmd =
      fmt::format("tar zxf \"{0}\" -C \"{1}\"", package_path, target_dir);
  SysUtil::System(cmd);
}

bool SysUtil::CheckMD5(const std::string& fname, const std::string& md5sum) {
  std::ifstream file_is(fname);
  std::string content((std::istreambuf_iterator<char>(file_is)),
                      std::istreambuf_iterator<char>());

  std::string md5_str = MD5String(content);
  if (md5_str.compare(md5sum) != 0) {
    SPDLOG_WARN("file({}) md5 check failed, expect:{}, get:{}", fname, md5sum,
                md5_str);
    return false;
  }
  return true;
}

}  // namespace secretflow::serving
