<div id="top" align="center">
<h1>OneDrive CLI (C++)</h1>

<p>A modern, cross-platform <b>OneDrive command-line client</b></p>

Report Issue](https://github.com/Zheng-Bote/onedrive_cli/issues) · [Request Feature](https://github.com/Zheng-Bote/onedrive_cli/pulls)

![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/Zheng-Bote/onedrive_cli?logo=GitHub)](https://github.com/Zheng-Bote/onedrive_cli/releases)

</div>

---

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Description](#description)
- [✨ Features](#-features)
- [📦 Requirements](#-requirements)
- [🔧 Build](#-build)
- [🔐 Azure App Registration](#-azure-app-registration)
  - [Required settings](#required-settings)
  - [Environment variable](#environment-variable)
- [🔑 Authentication](#-authentication)
- [📤 Upload single File](#-upload-single-file)
- [📥 Download single File](#-download-single-file)
- [📂 Upload Folder](#-upload-folder)
- [📥 Download Folder](#-download-folder)
- [🧠 Architecture Overview](#-architecture-overview)
  - [Implementation notes](#implementation-notes)
- [📜 License](#-license)
- [Authors](#authors)
  - [Code Contributors](#code-contributors)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

---

## Description

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)]()
[![CMake](https://img.shields.io/badge/CMake-3.23+-blue.svg)]()
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)

A modern, cross-platform **OneDrive command-line client** written in C++23, using **Microsoft Graph**, **OAuth 2.0 Device Code Flow**, and **libcurl**.

This project is designed as a clean, modular foundation for both CLI and future GUI (Qt) applications.

## ✨ Features

- 🔐 **OAuth 2.0 Device Code Flow** (no embedded browser, no client secrets)
- 🔄 **Automatic token refresh**
- 📤 **Upload files**
  - Small files via direct PUT
  - Large files (> 4 MB) via **Upload Sessions** (chunked)
- 📥 **Download files**
- 📂 **Folder upload** and **folder download** (recursive)
- 📊 **Progress display** with percentage, speed, and ETA
- 🧱 Clean separation of concerns:
  - **HttpClient** – HTTP transport
  - **AuthClient** – OAuth handling
  - **DriveClient** – OneDrive logic
- 🧩 Qt‑ready architecture (UI‑agnostic core)
- 🛠 Modern CMake build system
- 📦 Uses `nlohmann::json`

---

## 📦 Requirements

- C++23 compatible compiler (GCC ≥ 13, Clang ≥ 16)
- CMake ≥ 3.23
- libcurl (with HTTPS support)
- Internet connection
- Microsoft account (personal OneDrive)

---

## 🔧 Build

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

## 🔐 Azure App Registration

You must register an application in **Azure Active Directory**.

### Required settings

- Supported account types
  - ✔ Personal Microsoft accounts
  - or ✔ Personal + organizational accounts
- Authentication
  - ✔ Allow public client flows: Yes
  - Platform: Mobile and desktop applications
  - Redirect URI: (none required)
- API permissions (Delegated)
  - Files.ReadWrite
  - offline_access

### Environment variable

```bash
export ONEDRIVE_CLIENT_ID=<your-application-client-id>
```

## 🔑 Authentication

```bash
./onedrive_cli auth
```

You will be prompted to open a URL and enter a device code.

Tokens are stored at:

```Code
~/.config/onedrive_cli-cli/token.json
```

## 📤 Upload single File

```bash
./onedrive_cli upload local_file.txt docs/remote_file.txt
```

**_Example output_**

```Code
Uploading: 42% | 9.48 MB/s | ETA 00:18
Uploading: 100% | 9.51 MB/s | ETA 00:00
Uploaded.
```

- Files ≤ 4 MB use direct upload
- Files > 4 MB use Microsoft Graph Upload Sessions

## 📥 Download single File

```bash
./onedrive_cli download docs/remote_file.txt local_copy.txt
```

## 📂 Upload Folder

```bash
./onedrive_cli upload-folder <local_dir> <remote_dir> [--recursive] [--strip-root]
```

## 📥 Download Folder

```bash
./onedrive_cli download-folder <remote_dir> <local_dir> [--recursive] [--strip-root]
```

## 🧠 Architecture Overview

```Code
include/
└── onedrive
    ├── auth.hpp
    ├── drive_client.hpp
    ├── http_client.hpp
    ├── token_store.hpp
    └── util.hpp
src/
├── auth.cpp
├── cli.cpp
├── drive_client.cpp
├── http_client.cpp
└── token_store.cpp

CMakeLists.txt
```

- No UI logic in core components
- All progress reporting via callbacks
- Designed for reuse in Qt or other frontends

### Implementation notes

- TokenStore requires a file path at construction; the CLI uses ~/.config/onedrive-cli/token.json by default.
- DriveClient constructor in this codebase uses the signature:

```cpp
DriveClient(HttpClient &http, OAuthClient &auth);
```

The CLI constructs an HttpClient, an OAuthClient, then passes both to DriveClient.

- Listing API: DriveClient::list_folder(remote_path, recursive) returns entries with { path, size, is_dir }. The CLI maps those entries to local filesystem paths for downloads.
- Error handling: Basic retry/backoff is implemented for rate limits (HTTP 429). Network and API errors surface as exceptions; you can extend retry strategies or add resume support as needed.

---

## 📜 License

MIT License

© 2026 ZHENG Robert

## Authors

- [![Zheng Robert - Core Development](https://img.shields.io/badge/Github-Zheng_Robert-black?logo=github)](https://www.github.com/Zheng-Bote)

### Code Contributors

![Contributors](https://img.shields.io/github/contributors/Zheng-Bote/onedrive_cli?color=dark-green)

[![Zheng Robert](https://img.shields.io/badge/Github-Zheng_Robert-black?logo=github)](https://www.github.com/Zheng-Bote)

---

:vulcan_salute:

<p align="right">(<a href="#top">back to top</a>)</p>
