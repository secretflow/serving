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

#pragma once

#include <string>

namespace secretflow::serving {

class SysUtil {
 public:
  static void System(const std::string& cmd,
                     std::string* command_output = nullptr);

  static void ExtractGzippedArchive(const std::string& package_path,
                                    const std::string& target_dir);

  static bool CheckMD5(const std::string& fname, const std::string& md5sum);
};

}  // namespace secretflow::serving
