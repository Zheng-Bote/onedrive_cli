/**
 * SPDX-FileComment: OneDrive Token Storage Source
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file token_store.cpp
 * @brief Source for storing and retrieving auth tokens
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#include "onedrive/token_store.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace onedrive {

TokenStore::TokenStore(std::filesystem::path path) : path_(std::move(path)) {}

std::optional<TokenSet> TokenStore::load() const {
  std::ifstream is(path_);
  if (!is)
    return std::nullopt;

  nlohmann::json j;
  is >> j;

  TokenSet t;
  t.access_token = j.value("access_token", "");
  t.refresh_token = j.value("refresh_token", "");
  t.expires_at_unix = j.value("expires_at", 0LL);

  if (t.refresh_token.empty())
    return std::nullopt;
  return t;
}

void TokenStore::save(const TokenSet &t) const {
  std::filesystem::create_directories(path_.parent_path());

  nlohmann::json j{{"access_token", t.access_token},
                   {"refresh_token", t.refresh_token},
                   {"expires_at", t.expires_at_unix}};

  const auto tmp = path_.string() + ".tmp";
  {
    std::ofstream os(tmp, std::ios::trunc);
    os << j.dump(2) << "\n";
  }
  std::filesystem::rename(tmp, path_);
}

} // namespace onedrive
