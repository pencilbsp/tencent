// logging.cpp — Xem include/tencent/core/common/logging.h.
#include "tencent/core/common/logging.h"

#include <mutex>

namespace tencent {
namespace {

// Trạng thái toàn cục nhỏ gọn. Mutex chỉ bảo vệ lúc PHÁT sự kiện, để các luồng
// tải song song không xen kẽ dòng log của nhau.
LogSink& sink_storage() {
  static LogSink sink;   // mặc định rỗng = im lặng
  return sink;
}

LogLevel& level_storage() {
  static LogLevel level = LogLevel::kInfo;
  return level;
}

std::mutex& sink_mutex() {
  static std::mutex mutex;
  return mutex;
}

}  // namespace

void set_log_sink(LogSink sink) {
  std::lock_guard<std::mutex> lock(sink_mutex());
  sink_storage() = std::move(sink);
}

void set_log_level(LogLevel level) { level_storage() = level; }

LogLevel log_level() { return level_storage(); }

void log(LogLevel level, const std::string& message) {
  if (static_cast<int>(level) > static_cast<int>(level_storage())) return;
  std::lock_guard<std::mutex> lock(sink_mutex());
  if (sink_storage()) sink_storage()(level, message);
}

namespace {

// Phần dòng chưa gặp '\n'. Để riêng theo LUỒNG: các luồng tải chạy song song
// không được trộn nửa dòng của nhau.
std::string& pending_line() {
  thread_local std::string pending;
  return pending;
}

}  // namespace

LogStream::~LogStream() {
  std::string& pending = pending_line();
  pending += buffer_.str();

  // Phát mọi dòng ĐÃ HOÀN CHỈNH; phần đuôi chờ lần ghi sau.
  std::size_t newline = pending.find('\n');
  while (newline != std::string::npos) {
    std::string line = pending.substr(0, newline);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    log(level_, line);
    pending.erase(0, newline + 1);
    newline = pending.find('\n');
  }
}

void log_progress(const std::string& message) {
  // Đẩy nốt dòng dở dang trước, để tiến độ không dính vào giữa một dòng log.
  flush_log_lines();
  log(LogLevel::kInfo, "\r" + message);
}

void flush_log_lines() {
  std::string& pending = pending_line();
  if (pending.empty()) return;
  log(LogLevel::kInfo, pending);
  pending.clear();
}

}  // namespace tencent
