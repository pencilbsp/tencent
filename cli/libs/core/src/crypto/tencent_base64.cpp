// tencent_base64.cpp — Xem include/tencent/core/crypto/tencent_base64.h.
// Thân hàm giữ NGUYÊN từ bản đã verify byte-exact (src/tencent_crypto.cpp cũ).
#include "tencent/core/crypto/tencent_base64.h"

namespace tencent::crypto {
namespace {

constexpr char kB64Std[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Bảng giải mã ngược cho base64 CHUẨN (sau khi đã hoán '_'→'+', '-'→'/').
int b64_val(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;  // '=' hoặc ký tự lạ
}

}  // namespace

std::string tencent_b64_encode(const Bytes& data) {
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);
  std::size_t i = 0;
  for (; i + 3 <= data.size(); i += 3) {
    uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
    out.push_back(kB64Std[(n >> 18) & 0x3F]);
    out.push_back(kB64Std[(n >> 12) & 0x3F]);
    out.push_back(kB64Std[(n >> 6) & 0x3F]);
    out.push_back(kB64Std[n & 0x3F]);
  }
  const std::size_t rem = data.size() - i;
  if (rem == 1) {
    uint32_t n = uint32_t(data[i]) << 16;
    out.push_back(kB64Std[(n >> 18) & 0x3F]);
    out.push_back(kB64Std[(n >> 12) & 0x3F]);
    // 2 ký tự '=' bị bỏ (rstrip '=') — Tencent không giữ padding.
  } else if (rem == 2) {
    uint32_t n = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
    out.push_back(kB64Std[(n >> 18) & 0x3F]);
    out.push_back(kB64Std[(n >> 12) & 0x3F]);
    out.push_back(kB64Std[(n >> 6) & 0x3F]);
    // 1 ký tự '=' bị bỏ.
  }
  // Hoán ký tự 62/63 sang kiểu Tencent.
  for (char& c : out) {
    if (c == '+') c = '_';
    else if (c == '/') c = '-';
  }
  return out;
}

Bytes tencent_b64_decode(const std::string& text) {
  // Đảo hoán: '_' → '+', '-' → '/' (parse_ckey_v42.py:70).
  std::string standard;
  standard.reserve(text.size());
  for (char c : text) {
    if (c == '_') standard.push_back('+');
    else if (c == '-') standard.push_back('/');
    else standard.push_back(c);
  }
  // Gom 6 bit một, không cần '=' (phần dư xử lý tự nhiên qua bộ đệm bit).
  Bytes out;
  out.reserve(standard.size() / 4 * 3 + 2);
  uint32_t buffer = 0;
  int bits = 0;
  for (char c : standard) {
    const int value = b64_val(c);
    if (value < 0) continue;  // bỏ qua '=' / ký tự lạ
    buffer = (buffer << 6) | uint32_t(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
    }
  }
  return out;
}

}  // namespace tencent::crypto
