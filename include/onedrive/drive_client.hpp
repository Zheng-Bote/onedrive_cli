/**
 * SPDX-FileComment: OneDrive Drive Client Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file drive_client.hpp
 * @brief High-level OneDrive operations (upload/download/list)
 * @version 0.1.3
 * @date 2026-03-14
 *
 */

#pragma once

#include "onedrive/auth_client.hpp"
#include "onedrive/http_client.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace onedrive {

/**
 * @brief High level OneDrive client that implements upload/download and
 * listing.
 *
 * DriveClient holds references to an HttpClient and an OAuthClient.
 */
class DriveClient {
public:
  using ProgressFn =
      std::function<void(std::uint64_t /*sent*/, std::uint64_t /*total*/)>;

  struct RemoteEntry {
    std::string path;   /**< remote path, e.g. "docs/include/foo.txt" */
    std::uint64_t size; /**< size in bytes (0 for folders) */
    bool is_dir{false}; /**< true if entry is a folder */
  };

  /**
   * @brief Construct a DriveClient.
   *
   * @param http Reference to an HttpClient instance (must outlive this object).
   * @param auth Reference to an OAuthClient instance (must outlive this
   * object).
   */
  DriveClient(HttpClient &http, OAuthClient &auth);

  /**
   * @brief Upload a local file to a remote path. Chooses small or large upload.
   */
  void upload(const std::filesystem::path &local_file,
              std::string_view remote_path, ProgressFn progress = nullptr);

  /**
   * @brief Upload a large file using an upload session and chunked PUTs.
   */
  void upload_large(const std::filesystem::path &local, std::string_view remote,
                    std::size_t chunk_size, ProgressFn progress = nullptr);

  /**
   * @brief Download a remote file to a local path.
   */
  void download(std::string_view remote_path,
                const std::filesystem::path &local_file);

  /**
   * @brief List entries under a remote folder.
   */
  std::vector<RemoteEntry> list_folder(std::string_view remote_path,
                                       bool recursive);

private:
  std::string make_content_url(std::string_view remote_path) const;

  // Helper to call an HTTP lambda with current bearer token and refresh on 401.
  HttpResponse request_with_refresh_on_401(
      const std::function<HttpResponse(const std::string &)> &fn);

private:
  HttpClient &http_;
  OAuthClient &auth_;
};

} // namespace onedrive
