/**
 * SPDX-FileComment: OneDrive API Client Source
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file drive_client.cpp
 * @brief Source for high-level OneDrive operations
 * @version 0.1.3
 * @date 2026-03-14
 *
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

DriveClient::DriveClient(HttpClient &http, OAuthClient &auth)
    : http_(http), auth_(auth) {}

std::string DriveClient::make_content_url(std::string_view remote_path) const {
  // Graph path syntax: /me/drive/root:/path/to/file:/content
  std::string rp(remote_path);
  if (rp.empty() || rp[0] != '/')
    rp = "/" + rp;

  const auto encoded = url_encode(rp);
  return std::string(kGraph) + "/me/drive/root:" + encoded + ":/content";
}

HttpResponse DriveClient::request_with_refresh_on_401(
    const std::function<HttpResponse(const std::string &)> &fn) {
  auto token = auth_.access_token();
  auto r = fn(token);
  if (r.status == 401) {
    // Try to refresh and retry once
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
    // small upload (PUT ...:/content)
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
                               std::to_string(r.status) + "\n" + r.body);
    }
  } else {
    // large upload (Upload Session)
    upload_large(local_file, remote_path, 10 * 1024 * 1024, progress);
  }
}

/**
 * @brief Creates an upload session for a large file.
 */
static std::string create_upload_session(HttpClient &http,
                                         const std::string &token,
                                         std::string_view remote_path) {
  std::string rp(remote_path);
  if (rp.empty() || rp[0] != '/')
    rp = "/" + rp;

  const std::string url = std::string(kGraph) +
                          "/me/drive/root:" + url_encode(rp) +
                          ":/createUploadSession";

  std::vector<std::string> headers{"Authorization: Bearer " + token,
                                   "Content-Type: application/json"};

  // Graph requires JSON body; request minimal options
  const std::string body =
      R"({"item": {"@microsoft.graph.conflictBehavior": "replace"}})";

  auto r = http.post_json(url, body, headers);

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
  if (!is) {
    throw std::runtime_error("Failed to open file for large upload: " +
                             local.string());
  }

  std::vector<char> buffer(chunk_size);

  std::uint64_t offset = 0;
  while (offset < filesize) {
    const auto to_send = static_cast<std::size_t>(
        std::min<std::uint64_t>(chunk_size, filesize - offset));

    is.read(buffer.data(), static_cast<std::streamsize>(to_send));
    const auto actually_read = static_cast<std::size_t>(is.gcount());
    if (actually_read == 0)
      break;

    std::vector<std::string> headers{
        "Content-Length: " + std::to_string(actually_read),
        "Content-Range: bytes " + std::to_string(offset) + "-" +
            std::to_string(offset + actually_read - 1) + "/" +
            std::to_string(filesize)};

    auto r = http_.put_chunk(upload_url, buffer.data(), actually_read, headers);
    if (r.status >= 200 && r.status < 300) {
      // Completed upload session returned final item
      offset += actually_read;
      if (progress)
        progress(offset, filesize);
      break;
    } else if (r.status == 202) {
      // 202 Accepted indicates chunk accepted; continue
      offset += actually_read;
      if (progress)
        progress(offset, filesize);
    } else {
      // Retry logic for transient errors (simple exponential backoff)
      bool success = false;
      for (int attempt = 1; attempt <= 4; ++attempt) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(200 * (1 << attempt)));
        auto rr =
            http_.put_chunk(upload_url, buffer.data(), actually_read, headers);
        if (rr.status >= 200 && rr.status < 300) {
          success = true;
          break;
        }
        if (rr.status == 202) {
          success = true;
          break;
        }
      }
      if (!success) {
        throw std::runtime_error("Chunk upload failed at offset " +
                                 std::to_string(offset));
      }
      offset += actually_read;
      if (progress)
        progress(offset, filesize);
    }
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
                               std::to_string(r.status) + "\n" + r.body);
    }
    return;
  }
  throw std::runtime_error("Download failed: too many retries (429).");
}

/**
 * @brief Internal function to list a single folder (non-recursive). 
 * Returns raw JSON items.
 * 
 * @param http The HTTP client to use.
 * @param token The OAuth access token.
 * @param remote_path The remote path to list.
 * @return std::vector<nlohmann::json> The list of raw JSON items.
 */
static std::vector<nlohmann::json>
list_folder_raw(HttpClient &http, const std::string &token,
                std::string_view remote_path) {
  std::vector<nlohmann::json> out;
  std::string url;
  if (remote_path == std::string("/") || remote_path == std::string("")) {
    url = std::string(kGraph) + "/me/drive/root/children";
  } else {
    std::string rp(remote_path);
    if (rp.front() != '/')
      rp = "/" + rp;
    url =
        std::string(kGraph) + "/me/drive/root:" + url_encode(rp) + ":/children";
  }

  std::vector<std::string> headers{"Authorization: Bearer " + token};

  while (!url.empty()) {
    auto r = http.get(url, headers);
    if (r.status < 200 || r.status >= 300) {
      throw std::runtime_error("list_folder failed: HTTP " +
                               std::to_string(r.status) + "\n" + r.body);
    }
    auto j = nlohmann::json::parse(r.body);
    if (j.contains("value") && j["value"].is_array()) {
      for (auto &item : j["value"])
        out.push_back(item);
    }
    // handle pagination: @odata.nextLink
    if (j.contains("@odata.nextLink") && j["@odata.nextLink"].is_string()) {
      url = j["@odata.nextLink"].get<std::string>();
    } else {
      url.clear();
    }
  }

  return out;
}

/**
 * @brief Lists the contents of a folder on OneDrive.
 *
 * @param remote_path The remote path to list.
 * @param recursive If true, lists the folder recursively.
 * @return std::vector<DriveClient::RemoteEntry> List of entries.
 */
std::vector<DriveClient::RemoteEntry>
DriveClient::list_folder(std::string_view remote_path, bool recursive) {
  std::vector<RemoteEntry> results;

  // Obtain token and list
  auto token = auth_.access_token();

  std::function<void(std::string_view)> walk = [&](std::string_view rp) {
    auto items = list_folder_raw(http_, token, rp);
    for (auto &it : items) {
      const std::string name = it.value("name", "");
      bool is_folder = it.contains("folder");
      std::uint64_t size = 0;
      if (!is_folder) {
        size = it.value("size", 0ULL);
      }

      // Build child remote path relative to the requested root.
      std::string child_path;
      if (rp.empty() || rp == std::string("/")) {
        child_path = name;
      } else {
        std::string base(rp);
        if (!base.empty() && base.front() == '/')
          base.erase(base.begin());
        if (!base.empty())
          child_path = base + "/" + name;
        else
          child_path = name;
      }

      results.push_back(RemoteEntry{child_path, size, is_folder});

      if (is_folder && recursive) {
        // recurse into folder
        walk(child_path);
      }
    }
  };

  walk(remote_path);

  return results;
}

} // namespace onedrive
