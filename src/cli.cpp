/**
 * SPDX-FileComment: Command Line Interface Entry Point
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file cli.cpp
 * @brief Main entry point for the OneDrive CLI application
 * @version 0.1.0
 * @date 2026-03-11
 *
 * @author ZHENG Robert (robert@hase-zheng.net)
 * @copyright Copyright (c) 2026 ZHENG Robert
 *
 * @license MIT License
 */

#include "onedrive/auth.hpp"
#include "onedrive/drive_client.hpp"
#include "onedrive/util.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>

using namespace onedrive;

/**
 * @brief Prints the usage information to standard error.
 */
static void usage() {
  std::cerr << "Usage:\n"
               "  onedrive auth\n"
               "  onedrive upload <local_file> <remote_path>\n"
               "  onedrive download <remote_path> <local_file>\n\n"
               "Env:\n"
               "  ONEDRIVE_CLIENT_ID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx\n";
}

/**
 * @brief Main entry point of the CLI application.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 * @return int Exit status.
 */
int main(int argc, char **argv) {
  try {
    if (argc < 2) {
      usage();
      return 2;
    }

    const char *cid = std::getenv("ONEDRIVE_CLIENT_ID");
    if (!cid || !*cid) {
      std::cerr << "Missing ONEDRIVE_CLIENT_ID.\n";
      return 2;
    }

    const auto token_path = config_dir() / "token.json";
    OAuthClient auth{cid, TokenStore{token_path}};
    DriveClient drive{auth};

    const std::string cmd = argv[1];

    if (cmd == "auth") {
      auth.authenticate_device_flow();
      std::cout << "Token saved to: " << token_path << "\n";
      return 0;
    }

    if (cmd == "upload") {
      if (argc != 4) {
        usage();
        return 2;
      }
      std::filesystem::path local = argv[2];
      std::string remote = argv[3];
      using clock = std::chrono::steady_clock;

      const auto start = clock::now();
      std::uint64_t last_bytes = 0;
      auto last_time = start;

      drive.upload(local, remote, [&](std::uint64_t sent, std::uint64_t total) {
        const auto now = clock::now();
        const auto dt = std::chrono::duration<double>(now - last_time).count();

        double speed = 0.0;
        if (dt > 0.0) {
          speed = (sent - last_bytes) / dt;
        }

        const auto elapsed = std::chrono::duration<double>(now - start).count();

        const double avg_speed = elapsed > 0.0 ? sent / elapsed : 0.0;

        const auto remaining = total - sent;
        const auto eta_sec = avg_speed > 0.0 ? remaining / avg_speed : 0.0;

        const int pct = static_cast<int>((100.0 * sent) / total);

        std::cerr << "\rUploading: " << std::setw(3) << pct << "% | "
                  << std::fixed << std::setprecision(2)
                  << (avg_speed / (1024 * 1024)) << " MB/s | ETA ";

        const int eta = static_cast<int>(eta_sec);
        std::cerr << std::setw(2) << std::setfill('0') << (eta / 60) << ":"
                  << std::setw(2) << (eta % 60) << std::setfill(' ')
                  << std::flush;

        last_bytes = sent;
        last_time = now;
      });

      std::cerr << "\nUploaded.\n";

      return 0;
    }

    if (cmd == "download") {
      if (argc != 4) {
        usage();
        return 2;
      }
      std::string remote = argv[2];
      std::filesystem::path local = argv[3];
      drive.download(remote, local);
      std::cout << "Downloaded.\n";
      return 0;
    }

    usage();
    return 2;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
