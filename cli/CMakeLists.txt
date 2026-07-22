# CMakeLists.txt — Điểm lắp ráp của dự án. KHÔNG chứa logic build chi tiết:
# mọi thứ được tách ra cmake/*.cmake và CMakeLists.txt của từng thành phần.
#
# Cây thành phần (xem docs/ARCHITECTURE.md):
#   libs/platform  → tencent::platform  (trừu tượng hoá hệ điều hành)
#   libs/core      → tencent::core      (toàn bộ nghiệp vụ; GUI dùng lại được)
#   apps/cli       → tencent-cli        (chỉ parse tham số + in kết quả)
#   tests          → ctest byte-exact
cmake_minimum_required(VERSION 3.16)

option(TENCENT_CLI_STANDALONE
  "Link tĩnh phụ thuộc ngoài để phát hành CLI độc lập" OFF)
option(TENCENT_BUILD_TESTS "Build bộ test byte-exact" ON)

# Phải đặt TRƯỚC project() thì mới có tác dụng với ExternalProject/OpenSSL.
if(APPLE AND TENCENT_CLI_STANDALONE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING
    "Phiên bản macOS tối thiểu cho bản phát hành standalone")
endif()

project(tencent_cli
  VERSION 2.0.0
  DESCRIPTION "Tencent Video CLI — lõi tách rời, dùng chung cho CLI và GUI"
  LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(ProjectOptions)
include(Dependencies)
include(FFmpeg)

add_subdirectory(libs/platform)
add_subdirectory(libs/core)
add_subdirectory(apps/cli)

if(TENCENT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
