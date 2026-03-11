/**
 * SPDX-FileComment: HTTP Client Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file http_client.hpp
 * @brief Header for HTTP communication
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace onedrive {

/**
 * @brief Callback function type for reporting progress.
 */
using ProgressFn = std::function<void(std::uint64_t sent, std::uint64_t total)>;

/**
 * @brief Structure representing an HTTP response.
 */
struct HttpResponse {
  long status = 0;    /**< The HTTP status code */
  std::string body;   /**< The response body string */
};

/**
 * @brief Class handling HTTP communication.
 */
class HttpClient {
public:
  /**
   * @brief Constructs an HttpClient.
   */
  HttpClient();

  /**
   * @brief Destroys the HttpClient.
   */
  ~HttpClient();

  /**
   * @brief Performs an HTTP GET request.
   *
   * @param url The URL to fetch.
   * @param headers Additional HTTP headers.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse get(std::string_view url,
                   const std::vector<std::string> &headers);

  /**
   * @brief Performs an HTTP POST request with form-urlencoded data.
   *
   * @param url The URL to post to.
   * @param form_urlencoded The url-encoded form data.
   * @param headers Additional HTTP headers.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse post_form(std::string_view url, std::string_view form_urlencoded,
                         const std::vector<std::string> &headers);

  /**
   * @brief Performs an HTTP PUT request to upload a file.
   *
   * @param url The URL to put to.
   * @param file The file to upload.
   * @param headers Additional HTTP headers.
   * @param progress Optional progress callback.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse put_file(std::string_view url, const std::filesystem::path &file,
                        const std::vector<std::string> &headers,
                        ProgressFn progress = {});

  /**
   * @brief Performs an HTTP GET request and downloads to a file.
   *
   * @param url The URL to fetch.
   * @param file The file to save the response to.
   * @param headers Additional HTTP headers.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse get_to_file(std::string_view url,
                           const std::filesystem::path &file,
                           const std::vector<std::string> &headers);

  /**
   * @brief Performs an HTTP PUT request to upload a chunk of data.
   *
   * @param url The URL to put to.
   * @param data The data buffer to upload.
   * @param size The size of the data buffer.
   * @param headers Additional HTTP headers.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse put_chunk(std::string_view url, const void *data,
                         std::size_t size,
                         const std::vector<std::string> &headers);

  /**
   * @brief Performs an HTTP POST request with JSON data.
   *
   * @param url The URL to post to.
   * @param json_body The JSON body to post.
   * @param headers Additional HTTP headers.
   * @return HttpResponse The HTTP response.
   */
  HttpResponse post_json(std::string_view url, std::string_view json_body,
                         const std::vector<std::string> &headers);

private:
  void *curl_ = nullptr; // CURL*
};

} // namespace onedrive
