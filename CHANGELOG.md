# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [[1.0.0] - 2026-03-14](#100---2026-03-14)
  - [Added](#added)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

---

## [1.0.0] - 2026-03-14

### Added

- Initial project structure with `src/` and `include/` directories.
- Basic OAuth 2.0 Device Code Flow authentication (`auth.cpp`, `auth.hpp`).
- Token persistence functionality (`token_store.cpp`, `token_store.hpp`).
- HTTP client wrapper using libcurl (`http_client.cpp`, `http_client.hpp`).
- High-level OneDrive operations including file upload (small & large chunks) and download (`drive_client.cpp`, `drive_client.hpp`).
- Command-line interface to invoke core operations (`cli.cpp`).
- Utility functions for URL encoding and configuration paths (`util.hpp`).
- Progress reporting for file uploads via `HttpClient`.
- Doxygen-style documentation across all C++ source files following guidelines.
- `README.md` containing build instructions, features, requirements, and usage examples.
- `CMakeLists.txt` build configuration targeting C++23.
