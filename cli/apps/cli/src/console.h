// console.h — Xuất dữ liệu ra terminal.
//
// ĐÂY là nơi duy nhất trong toàn dự án được phép in ra stdout/stderr. Lõi phát
// sự kiện qua sink (core/common/logging.h); CLI cài sink ở đây và tự lo định
// dạng, còn GUI sau này sẽ cài sink khác mà không phải sửa lõi.
#pragma once

#include <cstdint>
#include <string>

#include "tencent/core/common/result.h"

namespace tencent::cli {

// Cài sink log của lõi để mọi thông điệp đi ra terminal:
//   • kError/kWarn → stderr
//   • còn lại      → stdout
//   • thông điệp bắt đầu bằng '\r' = cập nhật tiến độ, in đè trên cùng một dòng.
// `verbose` bật mức kDebug (mặc định kInfo).
void install_console_log_sink(bool verbose = false);

// In ra stdout/stderr. Dùng thay cho printf rải rác để dễ đổi cách hiển thị.
void print(const std::string& text);          // kèm xuống dòng
void print_raw(const std::string& text);      // không thêm gì
void print_error(const std::string& text);    // ra stderr, kèm xuống dòng

// In một Error theo dạng thống nhất: "lỗi: <message> (<detail>)".
void print_error(const Error& error);

// Chuyển Error thành mã thoát của tiến trình (0/1/2).
int report(const Error& error);

// ── Định dạng dùng lại giữa các lệnh ────────────────────────────────────────

// "2026-07-22 10:30 (còn 3 ngày)" / "(đã hết hạn)" / "(không rõ)".
std::string format_expiry(int64_t timestamp, int64_t now);

// "1.4 GB" — bọc util::human_bytes cho tiện dùng trong lệnh.
std::string format_bytes(uint64_t bytes);

}  // namespace tencent::cli
