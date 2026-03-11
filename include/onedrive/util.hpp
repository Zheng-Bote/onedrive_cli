/**
 * SPDX-FileComment: Utility Functions Header
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file util.hpp
 * @brief Header for utility functions
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#pragma once
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace onedrive {

/**
 * @brief Gets the configuration directory for the application.
 *
 * @return std::filesystem::path The path to the configuration directory.
 */
inline std::filesystem::path config_dir() {
#if defined(__APPLE__)
  const char *home = std::getenv("HOME");
  if (!home)
    return {};
  return std::filesystem::path(home) / "Library" / "Application Support" /
         "onedrive-cli";
#else
  const char *xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg && *xdg)
    return std::filesystem::path(xdg) / "onedrive-cli";
  const char *home = std::getenv("HOME");
  if (!home)
    return {};
  return std::filesystem::path(home) / ".config" / "onedrive-cli";
#endif
}

/**
 * @brief URL-encodes a string.
 *
 * @param s The string to encode.
 * @return std::string The URL-encoded string.
 */
inline std::string url_encode(std::string_view s) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);

  for (unsigned char c : std::string(s)) {
    const bool unreserved =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~' || c == '/' ||
        c == ':'; // keep path separators and colon for Graph path syntax

    if (unreserved)
      out.push_back(static_cast<char>(c));
    else {
      out.push_back('%');
      out.push_back(hex[(c >> 4) & 0xF]);
      out.push_back(hex[c & 0xF]);
    }
  }
  return out;
}

} // namespace onedrive
