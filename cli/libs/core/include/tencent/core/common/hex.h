// hex.h — Chuyển đổi hex ↔ nhị phân (BẢN DUY NHẤT trong dự án).
//
// Bản cũ có 3 hiện thực trùng nhau (tencent_crypto.cpp, turing_material.cpp,
// tencent_cli_main.cpp). Cả ba cùng ngữ nghĩa nên được gộp về đây:
//   • encode → chữ thường, không phân cách.
//   • decode → độ dài phải CHẴN và mọi ký tự phải hợp lệ, nếu không ném
//     std::invalid_argument (giữ đúng hành vi cũ, chỉ thống nhất loại ngoại lệ).
#pragma once

#include <string>

#include "tencent/core/common/bytes.h"

namespace tencent::util {

std::string hex_encode(const Bytes& data);
std::string hex_encode(const std::string& data);

// Trả về dạng Bytes (dùng cho khoá/khối crypto).
Bytes hex_to_bytes(const std::string& hex);

// Trả về dạng std::string (dùng cho blob nhị phân đi thẳng vào proto/file).
std::string hex_to_string(const std::string& hex);

// Kiểm tra nhanh: chuỗi toàn ký tự hex và độ dài chẵn (không ném).
bool is_hex(const std::string& text);

}  // namespace tencent::util
