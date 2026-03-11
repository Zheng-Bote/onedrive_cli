/**
 * SPDX-FileComment: OneDrive Authentication Source
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file auth.cpp
 * @brief Source for Authentication logic
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#include "onedrive/auth.hpp"
#include "onedrive/util.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>

namespace onedrive {

/** @brief OAuth 2.0 Tenant for personal accounts */
static constexpr std::string_view kTenant = "consumers";

/** @brief URL for acquiring the device code */
static constexpr std::string_view kDeviceCodeUrl =
    "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";

/** @brief URL for exchanging the device code for tokens */
static constexpr std::string_view kTokenUrl =
    "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";

/** @brief Scopes required for OneDrive access */
static constexpr std::string_view kScope = "offline_access Files.ReadWrite";

/**
 * @brief Encodes a key-value pair for application/x-www-form-urlencoded format.
 *
 * @param k The key.
 * @param v The value.
 * @return std::string The encoded key-value pair as "k=v".
 */
static std::string form_encode_kv(std::string_view k, std::string_view v) {
  return url_encode(k) + "=" + url_encode(v);
}

/**
 * @brief Joins a list of key-value pairs into a form-urlencoded string.
 *
 * @param kvs List of key-value pairs.
 * @return std::string The resulting form-urlencoded string.
 */
static std::string form_join(
    std::initializer_list<std::pair<std::string_view, std::string_view>> kvs) {
  std::string out;
  bool first = true;
  for (auto &[k, v] : kvs) {
    if (!first)
      out += "&";
    first = false;
    out += form_encode_kv(k, v);
  }
  return out;
}

std::int64_t OAuthClient::now_unix() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

OAuthClient::OAuthClient(std::string client_id, TokenStore store)
    : client_id_(std::move(client_id)), store_(std::move(store)) {}

std::string OAuthClient::access_token() {
  if (!loaded_) {
    if (auto t = store_.load()) {
      tokens_ = *t;
      loaded_ = true;
    } else {
      throw std::runtime_error("No token cache found. Run: onedrive auth");
    }
  }

  const auto now = now_unix();
  if (!tokens_.access_token.empty() && now < (tokens_.expires_at_unix - 60)) {
    return tokens_.access_token;
  }

  tokens_ = refresh();
  store_.save(tokens_);
  return tokens_.access_token;
}

void OAuthClient::authenticate_device_flow() {
  tokens_ = device_flow();
  loaded_ = true;
  store_.save(tokens_);
}

TokenSet OAuthClient::device_flow() {
  // 1) request device code
  const auto form =
      form_join({{"client_id", client_id_}, {"scope", std::string(kScope)}});

  auto r = http_.post_form(kDeviceCodeUrl, form, {});
  if (r.status < 200 || r.status >= 300) {
    throw std::runtime_error("Device code request failed: HTTP " +
                             std::to_string(r.status) + "\n" + r.body);
  }

  auto j = nlohmann::json::parse(r.body);
  const std::string device_code = j.at("device_code").get<std::string>();
  const std::string user_code = j.at("user_code").get<std::string>();
  const std::string verify_uri = j.at("verification_uri").get<std::string>();
  const int interval = j.value("interval", 5);
  const int expires_in = j.value("expires_in", 900);

  std::cout << "Open: " << verify_uri << "\n";
  std::cout << "Enter code: " << user_code << "\n";
  std::cout << "Waiting for authorization...\n";

  // 2) poll token endpoint
  const auto deadline = now_unix() + expires_in;
  while (now_unix() < deadline) {
    const auto poll_form = form_join(
        {{"client_id", client_id_},
         {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
         {"device_code", device_code}});

    auto tr = http_.post_form(kTokenUrl, poll_form, {});
    if (tr.status >= 200 && tr.status < 300) {
      auto tj = nlohmann::json::parse(tr.body);

      TokenSet t;
      t.access_token = tj.at("access_token").get<std::string>();
      t.refresh_token = tj.at("refresh_token").get<std::string>();
      const int expires = tj.value("expires_in", 3600);
      t.expires_at_unix = now_unix() + expires;
      std::cout << "Authorized.\n";
      return t;
    }

    // OAuth errors come as JSON
    try {
      auto ej = nlohmann::json::parse(tr.body);
      const std::string err = ej.value("error", "");
      if (err == "authorization_pending") {
        std::this_thread::sleep_for(std::chrono::seconds(interval));
        continue;
      }
      if (err == "slow_down") {
        std::this_thread::sleep_for(std::chrono::seconds(interval + 5));
        continue;
      }
      throw std::runtime_error("Device flow failed: " + err + "\n" + tr.body);
    } catch (...) {
      throw std::runtime_error("Device flow token poll failed: HTTP " +
                               std::to_string(tr.status) + "\n" + tr.body);
    }
  }

  throw std::runtime_error("Device flow timed out.");
}

TokenSet OAuthClient::refresh() {
  if (tokens_.refresh_token.empty()) {
    throw std::runtime_error("No refresh token available. Run: onedrive auth");
  }

  const auto form = form_join({{"client_id", client_id_},
                               {"grant_type", "refresh_token"},
                               {"refresh_token", tokens_.refresh_token},
                               {"scope", std::string(kScope)}});

  auto r = http_.post_form(kTokenUrl, form, {});
  if (r.status < 200 || r.status >= 300) {
    throw std::runtime_error("Refresh failed: HTTP " +
                             std::to_string(r.status) + "\n" + r.body);
  }

  auto j = nlohmann::json::parse(r.body);

  TokenSet t;
  t.access_token = j.at("access_token").get<std::string>();
  t.refresh_token =
      j.value("refresh_token", tokens_.refresh_token); // sometimes omitted
  const int expires = j.value("expires_in", 3600);
  t.expires_at_unix = now_unix() + expires;
  return t;
}

} // namespace onedrive
