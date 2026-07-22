// hex.cpp — Xem include/tencent/core/common/hex.h.
#include "tencent/core/common/hex.h"

#include <stdexcept>

namespace tencent::util {
namespace {

constexpr char kDigits[] = "0123456789abcdef";

// Giá trị của một ký tự hex, -1 nếu không hợp lệ.
int nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Thân chung của hai hàm decode: gọi `emit` cho từng byte giải được.
template <typename Emit>
void decode_into(const std::string& hex, Emit emit) {
  if (hex.size() % 2 != 0) throw std::invalid_argument("hex: độ dài phải chẵn");
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int high = nibble(hex[i]);
    const int low = nibble(hex[i + 1]);
    if (high < 0 || low < 0) throw std::invalid_argument("hex: ký tự không hợp lệ");
    emit(static_cast<uint8_t>((high << 4) | low));
  }
}

}  // namespace

std::string hex_encode(const Bytes& data) {
  std::string out;
  out.reserve(data.size() * 2);
  for (uint8_t byte : data) {
    out.push_back(kDigits[byte >> 4]);
    out.push_back(kDigits[byte & 0x0F]);
  }
  return out;
}

std::string hex_encode(const std::string& data) {
  std::string out;
  out.reserve(data.size() * 2);
  for (unsigned char byte : data) {
    out.push_back(kDigits[byte >> 4]);
    out.push_back(kDigits[byte & 0x0F]);
  }
  return out;
}

Bytes hex_to_bytes(const std::string& hex) {
  Bytes out;
  out.reserve(hex.size() / 2);
  decode_into(hex, [&out](uint8_t byte) { out.push_back(byte); });
  return out;
}

std::string hex_to_string(const std::string& hex) {
  std::string out;
  out.reserve(hex.size() / 2);
  decode_into(hex, [&out](uint8_t byte) { out.push_back(static_cast<char>(byte)); });
  return out;
}

bool is_hex(const std::string& text) {
  if (text.empty() || text.size() % 2 != 0) return false;
  for (char c : text) {
    if (nibble(c) < 0) return false;
  }
  return true;
}

}  // namespace tencent::util
