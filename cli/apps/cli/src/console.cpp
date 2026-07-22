// console.cpp — Xem console.h.
#include "console.h"

#include <cstdio>
#include <ctime>

#include "tencent/core/common/logging.h"
#include "tencent/core/common/strings.h"
#include "tencent/core/service/error.h"

namespace tencent::cli {
namespace {

// Dòng tiến độ trước đó có đang chiếm chỗ trên terminal không. Nếu có, phải
// xoá nó trước khi in một dòng bình thường, tránh để lại đuôi ký tự cũ.
bool progress_line_active = false;

void clear_progress_line() {
  if (!progress_line_active) return;
  std::fputs("\r\033[K", stdout);   // về đầu dòng + xoá tới cuối dòng
  progress_line_active = false;
}

}  // namespace

void install_console_log_sink(bool verbose) {
  set_log_level(verbose ? LogLevel::kDebug : LogLevel::kInfo);
  set_log_sink([](LogLevel level, const std::string& message) {
    if (!message.empty() && message.front() == '\r') {
      // Cập nhật tiến độ: in đè, không xuống dòng.
      std::fputs("\r\033[K", stdout);
      std::fputs(message.c_str() + 1, stdout);
      std::fflush(stdout);
      progress_line_active = true;
      return;
    }
    clear_progress_line();
    if (level == LogLevel::kError || level == LogLevel::kWarn) {
      std::fputs((message + "\n").c_str(), stderr);
    } else {
      std::fputs((message + "\n").c_str(), stdout);
      std::fflush(stdout);
    }
  });
}

void print(const std::string& text) {
  clear_progress_line();
  std::fputs((text + "\n").c_str(), stdout);
  std::fflush(stdout);
}

void print_raw(const std::string& text) {
  clear_progress_line();
  std::fputs(text.c_str(), stdout);
  std::fflush(stdout);
}

void print_error(const std::string& text) {
  clear_progress_line();
  std::fputs((text + "\n").c_str(), stderr);
}

void print_error(const Error& error) {
  print_error("lỗi: " + error.to_string());
}

int report(const Error& error) {
  print_error(error);
  return service::exit_code_for(error.code);
}

std::string format_expiry(int64_t timestamp, int64_t now) {
  if (timestamp <= 0) return "(không rõ)";
  char buffer[32];
  const std::time_t value = static_cast<std::time_t>(timestamp);
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&value));

  const int64_t remaining = timestamp - now;
  std::string relative;
  if (remaining <= 0) relative = " (đã hết hạn)";
  else if (remaining < 3600) relative = " (còn " + std::to_string(remaining / 60) + " phút)";
  else if (remaining < 86400) relative = " (còn " + std::to_string(remaining / 3600) + " giờ)";
  else relative = " (còn " + std::to_string(remaining / 86400) + " ngày)";
  return std::string(buffer) + relative;
}

std::string format_bytes(uint64_t bytes) { return util::human_bytes(bytes); }

}  // namespace tencent::cli
