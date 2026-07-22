// paths_posix.cpp — Hiện thực POSIX của include/tencent/platform/paths.h.
#include "tencent/platform/paths.h"

#include <pwd.h>
#include <unistd.h>

#include <cstdlib>

namespace tencent::platform {

std::string home_directory() {
  if (const char* home = std::getenv("HOME"); home && *home) return home;
  // Không có $HOME (chạy trong daemon/sandbox): lấy từ bảng passwd.
  if (const passwd* entry = ::getpwuid(::getuid()); entry && entry->pw_dir) {
    return entry->pw_dir;
  }
  return "";
}

std::string default_app_data_directory() {
  const std::string home = home_directory();
  if (home.empty()) return ".tencent";
  return home + "/.tencent";
}

}  // namespace tencent::platform
