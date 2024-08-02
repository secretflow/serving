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
#include <string_view>

#include "aws/s3/S3Client.h"

namespace secretflow::serving {

struct OssOptions {
  std::string endpoint;
  std::string access_key_id;
  std::string secret_key;
  std::string bucket;
  bool virtual_hosted = false;

  int connect_timeout_ms = 300000;
  int request_timeout_ms = 300000;

  OssOptions() = default;
  // Required by std::map as keys
  bool operator<(const OssOptions& other) const {
    return endpoint < other.endpoint || access_key_id < other.access_key_id ||
           secret_key < other.secret_key || bucket < other.bucket;
  }
};

class OssClient {
 public:
  OssClient(const OssOptions& option);

  // get file from oss.
  void GetFile(std::string_view object_path, std::string_view dst_path);

 private:
  OssOptions options_;
  std::shared_ptr<Aws::S3::S3Client> s3_client_;
};

}  // namespace secretflow::serving
