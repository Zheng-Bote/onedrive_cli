/**
 * SPDX-FileComment: OneDrive API Client Source
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file drive_client.cpp
 * @brief Source for high-level OneDrive operations
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#include "onedrive/drive_client.hpp"
#include "onedrive/util.hpp"

#include <chrono>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

namespace onedrive {

/** @brief Microsoft Graph API base URL */
static constexpr std::string_view kGraph = "https://graph.microsoft.com/v1.0";

DriveClient::DriveClient(OAuthClient &auth) : auth_(auth) {}

std::string DriveClient::make_content_url(std::string_view remote_path) const {
  // Graph path syntax: /me/drive/root:/path/to/file:/content
  // remote_path should start with "/" (we normalize)
  std::string rp(remote_path);
  if (rp.empty() || rp[0] != '/')
    rp = "/" + rp;

  const auto encoded = url_encode(rp);
  return std::string(kGraph) + "/me/drive/root:" + encoded + ":/content";
}

HttpResponse DriveClient::request_with_refresh_on_401(
    std::function<HttpResponse(const std::string &bearer)> fn) {
  auto token = auth_.access_token();
  auto r = fn(token);
  if (r.status == 401) {
    // force refresh by calling access_token again after expiring locally is not
    // exposed; simplest: re-run auth_.access_token() which refreshes if needed;
    // if token revoked, user must re-auth.
    token = auth_.access_token();
    r = fn(token);
  }
  return r;
}

void DriveClient::upload(const std::filesystem::path &local_file,
                         std::string_view remote_path, ProgressFn progress) {
  const auto filesize = std::filesystem::file_size(local_file);

  constexpr std::uint64_t SMALL_UPLOAD_LIMIT = 4ull * 1024 * 1024;

  if (filesize <= SMALL_UPLOAD_LIMIT) {
    // 🔹 kleiner Upload (PUT ...:/content)
    const auto url = make_content_url(remote_path);

    auto do_put = [&](const std::string &bearer) {
      std::vector<std::string> headers{
          "Authorization: Bearer " + bearer,
          "Content-Type: application/octet-stream"};
      return http_.put_file(url, local_file, headers, progress);
    };

    auto r = request_with_refresh_on_401(do_put);
    if (r.status < 200 || r.status >= 300) {
      throw std::runtime_error("Upload failed: HTTP " +
                               std::to_string(r.status));
    }
  } else {
    // 🔹 großer Upload (Upload‑Session)
    upload_large(local_file, remote_path,
                 10 * 1024 * 1024, // 10 MB Chunks
                 progress);
  }
}

/**
 * @brief Creates an upload session for a large file.
 *
 * @param http The HTTP client to use.
 * @param token The OAuth access token.
 * @param remote_path The remote path to upload to.
 * @return std::string The upload URL for the session.
 */
static std::string create_upload_session(HttpClient &http,
                                         const std::string &token,
                                         std::string_view remote_path) {
  std::string rp(remote_path);
  if (rp.empty() || rp[0] != '/')
    rp = "/" + rp;

  const std::string url =
      "https://graph.microsoft.com/v1.0/me/drive/root:" + url_encode(rp) +
      ":/createUploadSession";

  std::vector<std::string> headers{"Authorization: Bearer " + token};

  // Graph requires JSON, even if empty
  auto r = http.post_json(url, "{}", headers);

  if (r.status < 200 || r.status >= 300) {
    throw std::runtime_error("createUploadSession failed: HTTP " +
                             std::to_string(r.status) + "\n" + r.body);
  }

  auto j = nlohmann::json::parse(r.body);
  return j.at("uploadUrl").get<std::string>();
}

void DriveClient::upload_large(const std::filesystem::path &local,
                               std::string_view remote, std::size_t chunk_size,
                               ProgressFn progress) {
  const auto filesize = std::filesystem::file_size(local);
  const auto token = auth_.access_token();
  const auto upload_url = create_upload_session(http_, token, remote);

  std::ifstream is(local, std::ios::binary);
  std::vector<char> buffer(chunk_size);

  std::uint64_t offset = 0;
  while (offset < filesize) {
    const auto to_send = std::min<std::uint64_t>(chunk_size, filesize - offset);

    is.read(buffer.data(), to_send);

    std::vector<std::string> headers{
        "Content-Length: " + std::to_string(to_send),
        "Content-Range: bytes " + std::to_string(offset) + "-" +
            std::to_string(offset + to_send - 1) + "/" +
            std::to_string(filesize)};

    http_.put_chunk(upload_url, buffer.data(), to_send, headers);

    offset += to_send;
    if (progress)
      progress(offset, filesize);
  }
}

void DriveClient::download(std::string_view remote_path,
                           const std::filesystem::path &local_file) {
  const auto url = make_content_url(remote_path);

  auto do_get = [&](const std::string &bearer) {
    std::vector<std::string> headers{"Authorization: Bearer " + bearer};
    return http_.get_to_file(url, local_file, headers);
  };

  for (int attempt = 0; attempt < 5; ++attempt) {
    auto r = request_with_refresh_on_401(do_get);
    if (r.status == 429) {
      std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
      continue;
    }
    if (r.status < 200 || r.status >= 300) {
      throw std::runtime_error("Download failed: HTTP " +
                               std::to_string(r.status));
    }
    return;
  }
  throw std::runtime_error("Download failed: too many retries (429).");
}

} // namespace onedrive
