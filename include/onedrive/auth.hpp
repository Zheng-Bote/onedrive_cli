/**
 * SPDX-FileComment: OneDrive Authentication Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file auth.hpp
 * @brief Header for Authentication logic
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#pragma once
#include "onedrive/http_client.hpp"
#include "onedrive/token_store.hpp"
#include <chrono>
#include <string>

namespace onedrive {

/**
 * @brief Class responsible for handling OAuth 2.0 Device Code Flow authentication.
 */
class OAuthClient {
public:
  /**
   * @brief Constructs an OAuthClient.
   *
   * @param client_id The client ID for the application.
   * @param store The token store to use for caching tokens.
   */
  OAuthClient(std::string client_id, TokenStore store);

  /**
   * @brief Initiates the interactive device flow authentication.
   */
  void authenticate_device_flow(); // interactive

  /**
   * @brief Retrieves a valid access token, refreshing it if necessary.
   *
   * @return std::string The valid access token.
   */
  std::string access_token();      // auto refresh if needed

private:
  /**
   * @brief Refreshes the access token using the refresh token.
   *
   * @return TokenSet The new token set.
   */
  TokenSet refresh();

  /**
   * @brief Performs the device code flow.
   *
   * @return TokenSet The obtained token set.
   */
  TokenSet device_flow();

  /**
   * @brief Utility function to get the current UNIX timestamp in seconds.
   *
   * @return std::int64_t The current UNIX timestamp.
   */
  static std::int64_t now_unix();

private:
  std::string client_id_;
  TokenStore store_;
  HttpClient http_;
  TokenSet tokens_{};
  bool loaded_ = false;
};

} // namespace onedrive
