/**
 * SPDX-FileComment: Command Line Interface Entry Point
 * SPDX-FileType: SOURCE
 * SPDX-FileContributor: ZHENG Robert
 * SPDX-FileCopyrightText: 2026 ZHENG Robert
 * SPDX-License-Identifier: MIT
 *
 * @file cli.cpp
 * @brief Main entry point for the OneDrive CLI application
 * @version 0.3.1
 * @date 2026-03-14
 *
 */

#include "onedrive/auth_client.hpp"
#include "onedrive/drive_client.hpp"
#include "onedrive/http_client.hpp"
#include "onedrive/token_store.hpp"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using steady_clock_t = std::chrono::steady_clock;

/**
 * @brief Represents a local file found during directory scanning.
 */
struct LocalFile {
  std::filesystem::path absolute; /**< Absolute path to the file */
  std::filesystem::path relative; /**< Path relative to the scan root */
  std::uint64_t size{};           /**< File size in bytes */
};

/**
 * @brief Represents a remote entry (file or folder) on OneDrive.
 */
struct RemoteEntry {
  std::string path;   /**< Full remote path or relative path depending on DriveClient */
  std::uint64_t size{}; /**< Size in bytes (0 for folders) */
  bool is_dir{false}; /**< True if the entry represents a folder */
};

/**
 * @brief Scans a local directory for files.
 *
 * @param root The root directory to scan.
 * @param recursive Whether to scan recursively.
 * @return std::vector<LocalFile> List of files found.
 */
static std::vector<LocalFile> scan_directory(const std::filesystem::path &root,
                                             bool recursive) {
  std::vector<LocalFile> files;

  const auto root_abs = std::filesystem::absolute(root);

  auto handle_entry = [&](const std::filesystem::directory_entry &e) {
    if (!e.is_regular_file())
      return;

    LocalFile f;
    f.absolute = e.path();
    f.size = std::filesystem::file_size(e.path());

    auto rel = std::filesystem::relative(e.path(), root_abs);
    f.relative = rel;
    files.push_back(std::move(f));
  };

  if (recursive) {
    for (const auto &e :
         std::filesystem::recursive_directory_iterator(root_abs)) {
      handle_entry(e);
    }
  } else {
    for (const auto &e : std::filesystem::directory_iterator(root_abs)) {
      handle_entry(e);
    }
  }

  return files;
}

/**
 * @brief Prints the usage information to standard error.
 */
static void print_usage() {
  std::cerr << "Usage:\n"
            << "  onedrive auth\n"
            << "  onedrive upload <local_file> <remote_path>\n"
            << "  onedrive download <remote_path> <local_file>\n"
            << "  onedrive upload-folder <local_dir> <remote_dir> "
               "[--recursive] [--strip-root]\n"
            << "  onedrive download-folder <remote_dir> <local_dir> "
               "[--recursive] [--strip-root]\n";
}

/**
 * @brief Determines the default file path for the OAuth token.
 *
 * @return std::filesystem::path The token file path.
 */
static std::filesystem::path default_token_path() {
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME")) {
    return std::filesystem::path(xdg) / "onedrive-cli" / "token.json";
  }
  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".config" / "onedrive-cli" /
           "token.json";
  }
  return std::filesystem::path("token.json");
}

/**
 * @brief Removes the first path component from a given path.
 * Example: "onedrive/foo/bar" -> "foo/bar"
 * If the path has only one component, returns an empty path.
 *
 * @param p The path to modify.
 * @return std::filesystem::path The path without the first component.
 */
static std::filesystem::path
strip_first_component(const std::filesystem::path &p) {
  std::vector<std::filesystem::path> parts;
  for (const auto &elem : p) {
    parts.push_back(elem);
  }
  if (parts.size() <= 1) {
    return std::filesystem::path(); // empty
  }
  std::filesystem::path out;
  for (size_t i = 1; i < parts.size(); ++i) {
    out /= parts[i];
  }
  return out;
}

/**
 * @brief Ensures that parent directories for a given local path exist.
 *
 * @param p The local path.
 */
static void ensure_parent_dirs(const std::filesystem::path &p) {
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    // ignore errors here; download will fail later if necessary
  }
}

/**
 * @brief Main entry point of the CLI application.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 * @return int Exit status.
 */
int main(int argc, char **argv) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  try {
    onedrive::HttpClient http;

    const char *cid_env = std::getenv("ONEDRIVE_CLIENT_ID");
    if (!cid_env) {
      std::cerr << "Environment variable ONEDRIVE_CLIENT_ID is not set.\n";
      std::cerr << "Please set it to your Azure app client id and retry.\n";
      return 1;
    }
    const std::string client_id = cid_env;

    const auto token_path = default_token_path();
    onedrive::TokenStore store(token_path);

    onedrive::OAuthClient auth(http, client_id, store);

    // Construct DriveClient with HttpClient and OAuthClient (matches header).
    onedrive::DriveClient drive(http, auth);

    const std::string cmd = argv[1];

    if (cmd == "auth") {
      auth.authenticate_device_flow();
      std::cout << "Authorized.\n";
      return 0;
    }

    if (cmd == "upload") {
      if (argc != 4) {
        print_usage();
        return 1;
      }

      const std::filesystem::path local = argv[2];
      const std::string remote = argv[3];

      const auto start = steady_clock_t::now();
      std::uint64_t last_bytes = 0;
      auto last_time = start;

      drive.upload(local, remote, [&](std::uint64_t sent, std::uint64_t total) {
        const auto now = steady_clock_t::now();
        const auto dt = std::chrono::duration<double>(now - last_time).count();
        const auto elapsed = std::chrono::duration<double>(now - start).count();

        double inst_speed = 0.0;
        if (dt > 0.0)
          inst_speed = (sent - last_bytes) / dt;

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
        print_usage();
        return 1;
      }

      const std::string remote = argv[2];
      const std::filesystem::path local = argv[3];

      ensure_parent_dirs(local);
      drive.download(remote, local);
      std::cout << "Downloaded.\n";
      return 0;
    }

    if (cmd == "upload-folder") {
      // parse flags: allowed forms:
      // upload-folder <local_dir> <remote_dir> [--recursive] [--strip-root]
      if (argc < 4 || argc > 6) {
        print_usage();
        return 1;
      }

      const std::filesystem::path local_root = argv[2];
      const std::string remote_root = argv[3];

      bool recursive = false;
      bool strip_root = false;
      for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--recursive")
          recursive = true;
        else if (arg == "--strip-root")
          strip_root = true;
        else {
          std::cerr << "Unknown option: " << arg << "\n";
          print_usage();
          return 1;
        }
      }

      if (!std::filesystem::exists(local_root) ||
          !std::filesystem::is_directory(local_root)) {
        std::cerr << "Local directory does not exist: " << local_root << "\n";
        return 1;
      }

      auto files = scan_directory(local_root, recursive);
      if (files.empty()) {
        std::cout << "No files to upload.\n";
        return 0;
      }

      std::uint64_t total_bytes = 0;
      for (const auto &f : files)
        total_bytes += f.size;

      std::uint64_t global_uploaded = 0;

      std::cout << "Uploading " << files.size() << " files ("
                << (total_bytes / (1024 * 1024)) << " MB) from \""
                << local_root.string() << "\" to \"" << remote_root << "\""
                << (recursive ? " (recursive)" : "")
                << (strip_root ? " (strip-root)" : "") << "\n";

      for (const auto &f : files) {
        std::filesystem::path rel = f.relative;
        if (strip_root) {
          auto stripped = strip_first_component(rel);
          if (stripped.empty()) {
            rel = std::filesystem::path();
          } else {
            rel = stripped;
          }
        }

        std::filesystem::path remote_path_p =
            std::filesystem::path(remote_root);
        if (!rel.empty())
          remote_path_p /= rel;

        const auto remote_path = remote_path_p.generic_string();

        std::cout << "File: " << f.relative.generic_string() << " -> "
                  << remote_path << "\n";

        std::uint64_t last_bytes = 0;
        const auto start = steady_clock_t::now();
        auto last_time = start;

        drive.upload(
            f.absolute, remote_path,
            [&](std::uint64_t sent, std::uint64_t total) {
              const auto now = steady_clock_t::now();
              const auto dt =
                  std::chrono::duration<double>(now - last_time).count();
              const auto elapsed =
                  std::chrono::duration<double>(now - start).count();

              double inst_speed = 0.0;
              if (dt > 0.0)
                inst_speed = (sent - last_bytes) / dt;

              const double avg_speed = elapsed > 0.0 ? sent / elapsed : 0.0;

              const auto remaining = total - sent;
              const auto eta_sec =
                  avg_speed > 0.0 ? remaining / avg_speed : 0.0;

              const int pct = static_cast<int>((100.0 * sent) / total);

              const std::uint64_t delta = sent - last_bytes;
              global_uploaded += delta;
              last_bytes = sent;
              last_time = now;

              const double global_pct =
                  total_bytes > 0 ? (100.0 * global_uploaded) / total_bytes
                                  : 0.0;

              std::cerr << "\r[" << std::setw(3) << static_cast<int>(global_pct)
                        << "% total] " << std::setw(3) << pct << "% | "
                        << std::fixed << std::setprecision(2)
                        << (avg_speed / (1024 * 1024)) << " MB/s | ETA ";

              const int eta = static_cast<int>(eta_sec);
              std::cerr << std::setw(2) << std::setfill('0') << (eta / 60)
                        << ":" << std::setw(2) << (eta % 60)
                        << std::setfill(' ') << std::flush;
            });

        std::cerr << "\n";
      }

      std::cout << "Folder upload completed.\n";
      return 0;
    }

    if (cmd == "download-folder") {
      // download-folder <remote_dir> <local_dir> [--recursive] [--strip-root]
      if (argc < 4 || argc > 6) {
        print_usage();
        return 1;
      }

      const std::string remote_root = argv[2];
      const std::filesystem::path local_root = argv[3];

      bool recursive = false;
      bool strip_root = false;
      for (int i = 4; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--recursive")
          recursive = true;
        else if (arg == "--strip-root")
          strip_root = true;
        else {
          std::cerr << "Unknown option: " << arg << "\n";
          print_usage();
          return 1;
        }
      }

      // Ensure local root exists
      std::error_code ec;
      std::filesystem::create_directories(local_root, ec);

      // --- Get listing from DriveClient (DriveClient::RemoteEntry)
      auto drive_entries = drive.list_folder(remote_root, recursive);

      // Convert DriveClient::RemoteEntry -> local RemoteEntry
      std::vector<RemoteEntry> remote_files;
      remote_files.reserve(drive_entries.size());
      for (const auto &de : drive_entries) {
        remote_files.push_back(RemoteEntry{de.path, de.size, de.is_dir});
      }

      // Filter only files (skip directories)
      std::vector<RemoteEntry> files;
      for (const auto &e : remote_files) {
        if (!e.is_dir)
          files.push_back(e);
      }

      if (files.empty()) {
        std::cout << "No remote files to download.\n";
        return 0;
      }

      // Compute total bytes
      std::uint64_t total_bytes = 0;
      for (const auto &f : files)
        total_bytes += f.size;

      std::uint64_t global_downloaded = 0;

      std::cout << "Downloading " << files.size() << " files ("
                << (total_bytes / (1024 * 1024)) << " MB) from \""
                << remote_root << "\" to \"" << local_root.string() << "\""
                << (recursive ? " (recursive)" : "")
                << (strip_root ? " (strip-root)" : "") << "\n";

      for (const auto &e : files) {
        // e.path is assumed to be the remote path relative to remote_root or
        // absolute. We need the relative part to map into local_root.
        std::filesystem::path rel;
        {
          std::string rp = remote_root;
          if (!rp.empty() && rp.back() != '/')
            rp.push_back('/');

          if (!rp.empty() && e.path.rfind(rp, 0) == 0) {
            std::string suffix = e.path.substr(rp.size());
            rel = std::filesystem::path(suffix);
          } else {
            rel = std::filesystem::path(e.path);
          }
        }

        if (strip_root) {
          auto stripped = strip_first_component(rel);
          if (stripped.empty())
            rel = std::filesystem::path();
          else
            rel = stripped;
        }

        std::filesystem::path local_path = local_root;
        if (!rel.empty())
          local_path /= rel;

        ensure_parent_dirs(local_path);

        std::cout << "File: " << e.path << " -> " << local_path.generic_string()
                  << "\n";

        // Download file (DriveClient::download supports simple file download)
        drive.download(e.path, local_path);

        // Update global progress after file completed
        global_downloaded += e.size;

        const double global_pct =
            total_bytes > 0 ? (100.0 * global_downloaded) / total_bytes : 0.0;
        std::cerr << "\r[" << std::setw(3) << static_cast<int>(global_pct)
                  << "% total] " << std::setw(3) << 100 << "% | "
                  << "done" << std::flush;
        std::cerr << "\n";
      }

      std::cout << "Folder download completed.\n";
      return 0;
    }

    print_usage();
    return 1;
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
