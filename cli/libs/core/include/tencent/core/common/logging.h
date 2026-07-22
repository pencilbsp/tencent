// logging.h — Kênh thông báo của lõi.
//
// NGUYÊN TẮC: lõi KHÔNG BAO GIỜ ghi ra std::cout/std::cerr. Bản cũ in trực tiếp
// từ download_pipeline/qimei_client…, khiến GUI sau này không thể hiển thị tiến
// độ (và test thì bẩn output). Thay vào đó lõi phát sự kiện qua sink; CLI cài
// một sink in ra terminal, GUI cài sink đẩy vào cửa sổ log.
//
// Sink mặc định là RỖNG (nuốt hết) — im lặng là hành vi đúng cho thư viện.
#pragma once

#include <functional>
#include <sstream>
#include <string>

namespace tencent {

enum class LogLevel {
  kError = 0,
  kWarn,
  kInfo,
  kDebug,   // chi tiết cho người phát triển
  kTrace,   // rất chi tiết (dump từng segment/URL) — chỉ bật khi cần
};

using LogSink = std::function<void(LogLevel level, const std::string& message)>;

// Đặt sink toàn cục. Truyền sink rỗng để tắt hẳn. An toàn khi gọi nhiều lần;
// KHÔNG an toàn khi gọi song song với log() từ luồng khác — hãy cài đặt một
// lần lúc khởi động.
void set_log_sink(LogSink sink);

// Chỉ phát ra sự kiện có mức <= ngưỡng. Mặc định kInfo.
void set_log_level(LogLevel level);
LogLevel log_level();

void log(LogLevel level, const std::string& message);

inline void log_error(const std::string& message) { log(LogLevel::kError, message); }
inline void log_warn(const std::string& message) { log(LogLevel::kWarn, message); }
inline void log_info(const std::string& message) { log(LogLevel::kInfo, message); }
inline void log_debug(const std::string& message) { log(LogLevel::kDebug, message); }
inline void log_trace(const std::string& message) { log(LogLevel::kTrace, message); }

// ── Ghi log theo kiểu dòng chảy (thay thế std::cout << …) ───────────────────
//
// Dùng cho mã vốn được viết bằng std::cout: `log_out() << "a=" << a << "\n";`
//
// Điểm mấu chốt: sink nhận TỪNG DÒNG HOÀN CHỈNH. Phần chưa gặp '\n' được giữ
// lại trong bộ đệm riêng của luồng, nên nhiều câu lệnh nối tiếp nhau vẫn ghép
// đúng thành một dòng như khi in ra cout — không bị vỡ dòng.
class LogStream {
 public:
  explicit LogStream(LogLevel level) : level_(level) {}
  LogStream(LogStream&&) = default;
  LogStream(const LogStream&) = delete;
  LogStream& operator=(const LogStream&) = delete;
  ~LogStream();

  template <typename T>
  LogStream& operator<<(const T& value) {
    buffer_ << value;
    return *this;
  }

 private:
  LogLevel level_;
  std::ostringstream buffer_;
};

inline LogStream log_out() { return LogStream(LogLevel::kInfo); }
inline LogStream log_err() { return LogStream(LogLevel::kError); }

// Đẩy nốt phần dòng dở dang (nếu có) ra sink. Gọi khi kết thúc một tác vụ dài.
void flush_log_lines();

// ── Tiến độ ─────────────────────────────────────────────────────────────────
// Thông điệp GHI ĐÈ chính nó (tốc độ tải, số segment đã xong). Sink nhận được
// chuỗi có tiền tố '\r' và KHÔNG có '\n' ở cuối: CLI in đè trên cùng một dòng
// terminal, GUI dùng để cập nhật thanh tiến độ thay vì thêm dòng log mới.
void log_progress(const std::string& message);

}  // namespace tencent
