cmake_minimum_required(VERSION 3.23)

# cmake -B build -S . 
# cmake --build build -j$(nproc)


project(onedrive_cli
VERSION 0.1.0
DESCRIPTION "OneDrive CLI"
HOMEPAGE_URL "https://github.com/Zheng-Bote/onedrive_cli"
LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(CURL REQUIRED)

add_library(onedrive_lib
  src/http_client.cpp
  src/token_store.cpp
  src/auth.cpp
  src/drive_client.cpp
)
target_include_directories(onedrive_lib PUBLIC include)
target_link_libraries(onedrive_lib PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)

add_executable(onedrive src/cli.cpp)
target_link_libraries(onedrive PRIVATE onedrive_lib)
