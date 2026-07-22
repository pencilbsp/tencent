// bytes.h — Kiểu chuỗi byte dùng chung cho toàn bộ lõi.
//
// Quy ước trong dự án:
//   • `Bytes`       — dữ liệu NHỊ PHÂN có ngữ nghĩa (khoá, khối TLV, plaintext).
//   • `std::string` — vẫn dùng cho payload mạng/proto vì API bên dưới (libcurl,
//                     zlib, protobuf) đều nhận string; string trong C++ chứa
//                     được byte 0x00 nên an toàn.
// Hai hàm chuyển đổi bên dưới là cầu nối chuẩn giữa hai cách biểu diễn — đừng
// tự viết vòng lặp copy ở nơi khác.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tencent {

using Bytes = std::vector<uint8_t>;

// Sao chép nội dung string sang Bytes (giữ nguyên mọi byte, kể cả 0x00).
inline Bytes to_bytes(const std::string& text) {
  return Bytes(text.begin(), text.end());
}

// Chiều ngược lại.
inline std::string to_string(const Bytes& data) {
  return std::string(data.begin(), data.end());
}

}  // namespace tencent
