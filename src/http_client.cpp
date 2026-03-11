/**
 * SPDX-FileComment: HTTP Client Source
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file http_client.cpp
 * @brief Source for HTTP communication
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#include "onedrive/http_client.hpp"

#include <curl/curl.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace onedrive {

// -----------------------------------------------------------------------------
// Progress handling
// -----------------------------------------------------------------------------

/**
 * @brief Context structure for libcurl progress callback.
 */
struct ProgressCtx {
  ProgressFn fn; /**< The progress function to invoke */
};

/**
 * @brief libcurl progress callback.
 *
 * @param clientp Pointer to ProgressCtx.
 * @param dltotal Total bytes to download.
 * @param dlnow Uploaded bytes (used as downloaded).
 * @param ultotal Total bytes to upload.
 * @param ulnow Uploaded bytes.
 * @return int 0 to continue, non-zero to abort.
 */
static int progress_cb(void *clientp, curl_off_t /*dltotal*/,
                       curl_off_t /*dlnow*/, curl_off_t ultotal,
                       curl_off_t ulnow) {
  auto *ctx = static_cast<ProgressCtx *>(clientp);
  if (ctx && ctx->fn && ultotal > 0) {
    ctx->fn(static_cast<std::uint64_t>(ulnow),
            static_cast<std::uint64_t>(ultotal));
  }
  return 0;
}

// -----------------------------------------------------------------------------
// Write / read helpers
// -----------------------------------------------------------------------------

/**
 * @brief libcurl write callback to append received data into a std::string.
 *
 * @param ptr Pointer to received data.
 * @param size Size of one data element.
 * @param nmemb Number of elements.
 * @param userdata Pointer to the target std::string.
 * @return size_t Bytes processed.
 */
static size_t write_to_string(void *ptr, size_t size, size_t nmemb,
                              void *userdata) {
  auto *s = static_cast<std::string *>(userdata);
  s->append(static_cast<const char *>(ptr), size * nmemb);
  return size * nmemb;
}

/**
 * @brief libcurl write callback to append received data into an ofstream.
 *
 * @param ptr Pointer to received data.
 * @param size Size of one data element.
 * @param nmemb Number of elements.
 * @param userdata Pointer to the target std::ofstream.
 * @return size_t Bytes processed.
 */
static size_t write_to_file(void *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  auto *os = static_cast<std::ofstream *>(userdata);
  os->write(static_cast<const char *>(ptr),
            static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

/**
 * @brief Context structure for uploading via an ifstream.
 */
struct UploadCtx {
  std::ifstream is; /**< The input file stream */
};

/**
 * @brief libcurl read callback to fetch data from an ifstream.
 *
 * @param buffer Pointer to the destination buffer.
 * @param size Size of one element.
 * @param nitems Number of elements buffer can hold.
 * @param userdata Pointer to the UploadCtx.
 * @return size_t Bytes read.
 */
static size_t read_from_file(char *buffer, size_t size, size_t nitems,
                             void *userdata) {
  auto *ctx = static_cast<UploadCtx *>(userdata);
  ctx->is.read(buffer, static_cast<std::streamsize>(size * nitems));
  return static_cast<size_t>(ctx->is.gcount());
}

// -----------------------------------------------------------------------------
// Header helper
// -----------------------------------------------------------------------------

/**
 * @brief Constructs a libcurl header list from a vector of strings.
 *
 * @param headers Vector of string headers.
 * @return curl_slist* Pointer to the linked list of headers.
 */
static curl_slist *make_headers(const std::vector<std::string> &headers) {
  curl_slist *list = nullptr;
  for (const auto &h : headers) {
    list = curl_slist_append(list, h.c_str());
  }
  return list;
}

// -----------------------------------------------------------------------------
// HttpClient lifecycle
// -----------------------------------------------------------------------------

HttpClient::HttpClient() {
  static const int init = [] {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
  }();
  (void)init;

  curl_ = curl_easy_init();
  if (!curl_) {
    throw std::runtime_error("curl_easy_init failed");
  }
}

HttpClient::~HttpClient() {
  if (curl_) {
    curl_easy_cleanup(static_cast<CURL *>(curl_));
  }
}

// -----------------------------------------------------------------------------
// Core perform helper
// -----------------------------------------------------------------------------

/**
 * @brief Executes a pre-configured libcurl request.
 *
 * @param curl The CURL instance.
 * @param hdrs The headers to attach to the request.
 * @param out_body Optional pointer to a string where the response body will be stored.
 * @return HttpResponse The result of the HTTP request.
 */
static HttpResponse perform(CURL *curl, curl_slist *hdrs,
                            std::string *out_body) {
  HttpResponse r;

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "onedrive-cli/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

  if (out_body) {
    out_body->clear();
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_body);
  }

  const auto rc = curl_easy_perform(curl);
  if (rc != CURLE_OK) {
    throw std::runtime_error(std::string("curl_easy_perform: ") +
                             curl_easy_strerror(rc));
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
  if (out_body) {
    r.body = *out_body;
  }
  return r;
}

// -----------------------------------------------------------------------------
// GET (string)
// -----------------------------------------------------------------------------

HttpResponse HttpClient::get(std::string_view url,
                             const std::vector<std::string> &headers) {
  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

  std::string body;
  auto *hdrs = make_headers(headers);
  auto r = perform(curl, hdrs, &body);
  curl_slist_free_all(hdrs);

  return r;
}

// -----------------------------------------------------------------------------
// POST form-urlencoded
// -----------------------------------------------------------------------------

HttpResponse HttpClient::post_form(std::string_view url,
                                   std::string_view form_urlencoded,
                                   const std::vector<std::string> &headers) {
  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  std::string body(form_urlencoded);

  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());

  std::vector<std::string> hdrs2 = headers;
  hdrs2.emplace_back("Content-Type: application/x-www-form-urlencoded");

  std::string response;
  auto *hdrs = make_headers(hdrs2);
  auto r = perform(curl, hdrs, &response);
  curl_slist_free_all(hdrs);

  r.body = std::move(response);
  return r;
}

// -----------------------------------------------------------------------------
// PUT small file (PUT ...:/content)
// -----------------------------------------------------------------------------

HttpResponse HttpClient::put_file(std::string_view url,
                                  const std::filesystem::path &file,
                                  const std::vector<std::string> &headers,
                                  ProgressFn progress) {
  if (!std::filesystem::exists(file)) {
    throw std::runtime_error("file not found: " + file.string());
  }

  UploadCtx ctx;
  ctx.is.open(file, std::ios::binary);
  if (!ctx.is) {
    throw std::runtime_error("cannot open file: " + file.string());
  }

  const auto size = static_cast<curl_off_t>(std::filesystem::file_size(file));

  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_from_file);
  curl_easy_setopt(curl, CURLOPT_READDATA, &ctx);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, size);

  ProgressCtx pctx{progress};
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pctx);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

  std::string body;
  auto *hdrs = make_headers(headers);
  auto r = perform(curl, hdrs, &body);
  curl_slist_free_all(hdrs);

  return r;
}

// -----------------------------------------------------------------------------
// GET to file (download)
// -----------------------------------------------------------------------------

HttpResponse HttpClient::get_to_file(std::string_view url,
                                     const std::filesystem::path &file,
                                     const std::vector<std::string> &headers) {
  std::filesystem::create_directories(
      file.parent_path().empty() ? "." : file.parent_path());

  std::ofstream os(file, std::ios::binary | std::ios::trunc);
  if (!os) {
    throw std::runtime_error("cannot open output file: " + file.string());
  }

  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  auto *hdrs = make_headers(headers);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_file);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &os);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "onedrive-cli/1.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

  const auto rc = curl_easy_perform(curl);
  curl_slist_free_all(hdrs);

  if (rc != CURLE_OK) {
    throw std::runtime_error(std::string("curl_easy_perform: ") +
                             curl_easy_strerror(rc));
  }

  HttpResponse r;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &r.status);
  return r;
}

// -----------------------------------------------------------------------------
// PUT chunk (upload session)
// -----------------------------------------------------------------------------

HttpResponse HttpClient::put_chunk(std::string_view url, const void *data,
                                   std::size_t size,
                                   const std::vector<std::string> &headers) {
  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, size);

  std::vector<std::string> hdrs2 = headers;
  hdrs2.emplace_back("Content-Length: " + std::to_string(size));

  std::string response;
  auto *hdrs = make_headers(hdrs2);
  auto r = perform(curl, hdrs, &response);
  curl_slist_free_all(hdrs);

  r.body = std::move(response);
  return r;
}

// -----------------------------------------------------------------------------
// POST JSON
// -----------------------------------------------------------------------------

HttpResponse HttpClient::post_json(std::string_view url,
                                   std::string_view json_body,
                                   const std::vector<std::string> &headers) {
  auto *curl = static_cast<CURL *>(curl_);
  curl_easy_reset(curl);

  std::string body(json_body);

  curl_easy_setopt(curl, CURLOPT_URL, std::string(url).c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());

  std::vector<std::string> hdrs2 = headers;
  hdrs2.emplace_back("Content-Type: application/json");

  std::string response;
  auto *hdrs = make_headers(hdrs2);
  auto r = perform(curl, hdrs, &response);
  curl_slist_free_all(hdrs);

  r.body = std::move(response);
  return r;
}

} // namespace onedrive
