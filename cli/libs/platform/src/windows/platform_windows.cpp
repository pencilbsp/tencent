// platform_windows.cpp — Khung hiện thực cho Windows (CHƯA hoàn thiện).
//
// Mục đích của file này là giữ RANH GIỚI: mọi thứ phụ thuộc hệ điều hành đã
// được gom vào đúng 3 header (process/paths/socket), nên việc bổ sung Windows
// chỉ là điền vào đây — không phải rải #ifdef khắp lõi.
//
// Trạng thái từng phần:
//   • paths   — ĐÃ CHẠY (%USERPROFILE%).
//   • process — CHƯA: cần CreateProcessW + hai anonymous pipe, đọc chồng lấp
//               (overlapped) hoặc 2 luồng đọc để không deadlock.
//   • socket  — CHƯA: cần WSAStartup + WSAPoll; phần còn lại ánh xạ 1:1 với
//               bản POSIX (xem src/posix/socket_posix.cpp).
// Khi hiện thực, PHẢI giữ nguyên ngữ nghĩa mô tả trong header.
#include "tencent/platform/paths.h"
#include "tencent/platform/process.h"
#include "tencent/platform/socket.h"

#include <cstdlib>
#include <stdexcept>

namespace tencent::platform {
namespace {
[[noreturn]] void not_implemented(const char* what) {
  throw std::runtime_error(std::string("Chưa hỗ trợ trên Windows: ") + what);
}
}  // namespace

// ── paths ────────────────────────────────────────────────────────────────────
std::string home_directory() {
  if (const char* profile = std::getenv("USERPROFILE"); profile && *profile) return profile;
  const char* drive = std::getenv("HOMEDRIVE");
  const char* path = std::getenv("HOMEPATH");
  if (drive && path) return std::string(drive) + path;
  return "";
}

std::string default_app_data_directory() {
  const std::string home = home_directory();
  if (home.empty()) return ".tencent";
  return home + "\\.tencent";
}

// ── process ──────────────────────────────────────────────────────────────────
ProcessResult run_process(const std::string& program, const std::vector<std::string>&, int) {
  ProcessResult result;
  result.spawned = false;
  result.stderr_data = "Chưa hỗ trợ chạy tiến trình con trên Windows: " + program;
  return result;
}

bool program_exists(const std::string&) { return false; }

// ── socket ───────────────────────────────────────────────────────────────────
void socket_startup() { not_implemented("socket_startup (WSAStartup)"); }
SocketHandle connect_tcp(const std::string&, uint16_t, long) { not_implemented("connect_tcp"); }
SocketHandle listen_loopback(uint16_t&) { not_implemented("listen_loopback"); }
SocketHandle accept_connection(SocketHandle) { not_implemented("accept_connection"); }
bool send_all(SocketHandle, const uint8_t*, std::size_t) { not_implemented("send_all"); }
bool recv_exact(SocketHandle, uint8_t*, std::size_t) { not_implemented("recv_exact"); }
long recv_some(SocketHandle, uint8_t*, std::size_t) { not_implemented("recv_some"); }
void shutdown_write(SocketHandle) {}
void shutdown_both(SocketHandle) {}
void close_socket(SocketHandle) {}

}  // namespace tencent::platform
