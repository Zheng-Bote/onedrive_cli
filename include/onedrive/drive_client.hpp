/**
 * SPDX-FileComment: OneDrive API Client Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file drive_client.hpp
 * @brief Header for high-level OneDrive operations
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#pragma once
#include "onedrive/auth.hpp"
#include "onedrive/http_client.hpp"

#include <filesystem>
#include <functional>
#include <string_view>

namespace onedrive {

/**
 * @brief Class representing the OneDrive logic and operations.
 */
class DriveClient {
public:
  /**
   * @brief Constructs a DriveClient.
   *
   * @param auth The OAuthClient to use for authentication.
   */
  explicit DriveClient(OAuthClient &auth);

  /**
   * @brief Uploads a file to OneDrive. Uses sessions for large files.
   *
   * @param local_file The local file to upload.
   * @param remote_path The remote path on OneDrive.
   * @param progress Optional progress callback.
   */
  void upload(const std::filesystem::path &local_file,
              std::string_view remote_path, ProgressFn progress = {});

  /**
   * @brief Downloads a file from OneDrive.
   *
   * @param remote_path The remote path on OneDrive.
   * @param local_file The local file to save as.
   */
  void download(std::string_view remote_path,
                const std::filesystem::path &local_file);

  /**
   * @brief Uploads a large file in chunks using an upload session.
   *
   * @param local_file The local file to upload.
   * @param remote_path The remote path on OneDrive.
   * @param chunk_size The chunk size in bytes (default 10MB).
   * @param progress Optional progress callback.
   */
  void upload_large(const std::filesystem::path &local_file,
                    std::string_view remote_path,
                    std::size_t chunk_size = 10 * 1024 * 1024,
                    ProgressFn progress = {});

private:
  /**
   * @brief Generates the upload/download API URL for a specific remote path.
   *
   * @param remote_path The remote path in OneDrive.
   * @return std::string The API URL.
   */
  std::string make_content_url(std::string_view remote_path) const;

  /**
   * @brief Executes an HTTP request with automatic token refresh on 401 error.
   *
   * @param fn The request function taking a bearer token.
   * @return HttpResponse The result of the HTTP request.
   */
  HttpResponse request_with_refresh_on_401(
      std::function<HttpResponse(const std::string &bearer)> fn);

private:
  OAuthClient &auth_;
  HttpClient http_;
};

} // namespace onedrive
