/**
 * SPDX-FileComment: OneDrive Token Storage Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file token_store.hpp
 * @brief Header for storing and retrieving auth tokens
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
#include <optional>
#include <string>

namespace onedrive {

/**
 * @brief Structure representing a set of OAuth tokens.
 */
struct TokenSet {
  std::string access_token;         /**< The access token string */
  std::string refresh_token;        /**< The refresh token string */
  std::int64_t expires_at_unix = 0; /**< Expiration time in seconds since epoch */
};

/**
 * @brief Class responsible for storing and retrieving token sets.
 */
class TokenStore {
public:
  /**
   * @brief Constructs a TokenStore with a specific file path.
   *
   * @param path The file path to store the tokens.
   */
  explicit TokenStore(std::filesystem::path path);

  /**
   * @brief Loads the token set from the store if it exists.
   *
   * @return std::optional<TokenSet> The loaded token set or nullopt.
   */
  std::optional<TokenSet> load() const;

  /**
   * @brief Saves a token set to the store.
   *
   * @param t The token set to save.
   */
  void save(const TokenSet &t) const;

  /**
   * @brief Gets the path where tokens are stored.
   *
   * @return const std::filesystem::path& The path to the token file.
   */
  const std::filesystem::path &path() const { return path_; }

private:
  std::filesystem::path path_;
};

} // namespace onedrive
