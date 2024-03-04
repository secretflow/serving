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

#include "secretflow_serving/util/oss_client.h"

#include "aws/core/Aws.h"
#include "aws/core/auth/AWSCredentials.h"
#include "aws/core/utils/threading/Executor.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/S3Errors.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/transfer/TransferManager.h"
#include "spdlog/spdlog.h"
#include "yacl/base/exception.h"

namespace secretflow::serving {

namespace {
const Aws::Http::Scheme kS3Scheme = Aws::Http::Scheme::HTTP;
const size_t kExecutorPoolSize = 4;
// Multipart upload require:
// Part number must be an integer between 1 and 10000, inclusive
// so, the largest Upload file size is 10000 * 10MB = 100GB
// https://docs.aws.amazon.com/AmazonS3/latest/dev/qfacts.html
const uint64_t kDownloadTransferBufferSize = 1024 * 1024;  // 1MB

void ShutdownClient(Aws::S3::S3Client* s3_client) {
  if (s3_client != nullptr) {
    delete s3_client;
  }
}

}  // namespace

OssClient::OssClient(const OssOptions& options) : options_(options) {
  YACL_ENFORCE(!options.endpoint.empty(), "empty endpoint for oss client");

  Aws::SDKOptions sdkoptions;
  Aws::InitAPI(sdkoptions);

  Aws::Client::ClientConfiguration cfg;
  cfg.endpointOverride = Aws::String(options.endpoint);
  cfg.scheme = kS3Scheme;
  cfg.verifySSL = false;
  cfg.connectTimeoutMs = options_.connect_timeout_ms;
  cfg.requestTimeoutMs = options_.request_timeout_ms;

  s3_client_ = std::shared_ptr<Aws::S3::S3Client>(
      new Aws::S3::S3Client(
          Aws::Auth::AWSCredentials(options.access_key_id.c_str(),
                                    options.secret_key.c_str()),
          cfg, Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
          options.virtual_hosted),
      ShutdownClient);
}

void OssClient::GetFile(std::string_view object_path,
                        std::string_view dst_path) {
  auto executor = Aws::MakeShared<Aws::Utils::Threading::PooledThreadExecutor>(
      "pool", kExecutorPoolSize);
  Aws::Transfer::TransferManagerConfiguration transferConfig(executor.get());
  transferConfig.s3Client = s3_client_;
  transferConfig.bufferSize = kDownloadTransferBufferSize;

  transferConfig.transferStatusUpdatedCallback =
      [](const Aws::Transfer::TransferManager*,
         const std::shared_ptr<const Aws::Transfer::TransferHandle>& handle) {
        SPDLOG_INFO("Transfer Status = {}",
                    static_cast<int>(handle->GetStatus()));
      };

  auto transferManager = Aws::Transfer::TransferManager::Create(transferConfig);
  Aws::Transfer::DownloadConfiguration downloadConf;
  // download to current directory
  auto transferHandle =
      transferManager->DownloadFile(options_.bucket.data(), object_path.data(),
                                    dst_path.data(), downloadConf, nullptr);
  transferHandle->WaitUntilFinished();

  auto status = transferHandle->GetStatus();
  YACL_ENFORCE(
      status == Aws::Transfer::TransferStatus::COMPLETED,
      "download file error, Aws error code:{}, msg:{}, bucket:{}, "
      "object_path:{}, "
      "dst_path:{}, "
      "endpoint:{}, virtual_hosted:{}",
      static_cast<int>(transferHandle->GetLastError().GetResponseCode()),
      transferHandle->GetLastError().GetMessage(), options_.bucket, object_path,
      dst_path, options_.endpoint, options_.virtual_hosted);
}

}  // namespace secretflow::serving
