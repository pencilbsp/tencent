# Dependencies.cmake — Tìm/dựng phụ thuộc ngoài (zlib, libcurl, OpenSSL).
#
# Kết quả là các target đã chuẩn hoá để phần còn lại của dự án chỉ việc link:
#   ZLIB::ZLIB, CURL::libcurl, Threads::Threads, tencent::openssl_crypto
#
# TENCENT_CLI_STANDALONE=ON: tự tải & build OpenSSL tĩnh để phát hành binary
# không phụ thuộc thư viện hệ thống (hiện chỉ cấu hình cho macOS).

include_guard(GLOBAL)
include(ExternalProject)

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

if(NOT TENCENT_CLI_STANDALONE)
  # ── Đường thường: dùng OpenSSL của hệ thống ────────────────────────────────
  find_package(OpenSSL REQUIRED)
  if(TARGET OpenSSL::Crypto)
    add_library(tencent_openssl_crypto INTERFACE)
    target_link_libraries(tencent_openssl_crypto INTERFACE OpenSSL::Crypto)
  else()
    # Một số bản CMake cũ chỉ cấp biến, không cấp target.
    add_library(tencent_openssl_crypto UNKNOWN IMPORTED GLOBAL)
    set_target_properties(tencent_openssl_crypto PROPERTIES
      IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}")
  endif()
else()
  # ── Đường standalone: build libcrypto tĩnh từ source đã ghim hash ──────────
  if(NOT APPLE)
    message(FATAL_ERROR
      "TENCENT_CLI_STANDALONE hiện chỉ được cấu hình cho macOS")
  endif()
  find_program(MAKE_EXECUTABLE make REQUIRED)

  if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64")
    set(_openssl_target darwin64-x86_64-cc)
  else()
    set(_openssl_target darwin64-arm64-cc)
  endif()

  set(_openssl_prefix "${CMAKE_BINARY_DIR}/openssl")
  set(_openssl_lib "${_openssl_prefix}/lib/libcrypto.a")
  file(MAKE_DIRECTORY "${_openssl_prefix}/include")

  ExternalProject_Add(openssl_external
    URL "https://github.com/openssl/openssl/releases/download/openssl-3.6.2/openssl-3.6.2.tar.gz"
    URL_HASH "SHA256=aaf51a1fe064384f811daeaeb4ec4dce7340ec8bd893027eee676af31e83a04f"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
      "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
      <SOURCE_DIR>/Configure "${_openssl_target}"
      no-shared no-tests no-module
      "--prefix=${_openssl_prefix}" "--openssldir=${_openssl_prefix}/ssl"
      "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    BUILD_COMMAND "${MAKE_EXECUTABLE}" -j4
    INSTALL_COMMAND "${MAKE_EXECUTABLE}" install_sw
    BUILD_BYPRODUCTS "${_openssl_lib}"
    UPDATE_COMMAND "")

  add_library(tencent_openssl_crypto STATIC IMPORTED GLOBAL)
  set_target_properties(tencent_openssl_crypto PROPERTIES
    IMPORTED_LOCATION "${_openssl_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "${_openssl_prefix}/include")
  add_dependencies(tencent_openssl_crypto openssl_external)
endif()

add_library(tencent::openssl_crypto ALIAS tencent_openssl_crypto)

# ── Thư viện header-only lấy từ submodule ────────────────────────────────────
# third_party/CLI11 (v2.4.2) và third_party/json (v3.11.3) là git submodule;
# xem .gitmodules ở repo gốc. CLI11 không commit sẵn bản single-header nên ta
# dùng dạng nhiều header: <CLI/CLI.hpp>.
# Include dạng SYSTEM để cảnh báo -Wall/-Wextra của ta không nổ trong code người khác.
set(_third_party_dirs
  "${CMAKE_SOURCE_DIR}/third_party/CLI11/include"
  "${CMAKE_SOURCE_DIR}/third_party/json/single_include")

foreach(_dir IN LISTS _third_party_dirs)
  if(NOT EXISTS "${_dir}")
    message(FATAL_ERROR
      "Thiếu ${_dir}. Chạy: git submodule update --init --recursive")
  endif()
endforeach()

add_library(tencent_third_party_headers INTERFACE)
target_include_directories(tencent_third_party_headers SYSTEM INTERFACE
  ${_third_party_dirs})
add_library(tencent::third_party_headers ALIAS tencent_third_party_headers)
