// rc4.cpp — Xem include/tencent/core/crypto/rc4.h.
// Thân hàm giữ NGUYÊN từ bản đã verify byte-exact (src/login_sign.cpp cũ).
#include "tencent/core/crypto/rc4.h"

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace tencent::crypto {

std::string rc4_cyclic8(const std::string& key, const std::string& data) {
  if (key.empty()) throw std::invalid_argument("rc4_cyclic8: khoá rỗng");

  int S[256];
  for (int i = 0; i < 256; ++i) S[i] = i;
  int j = 0;
  for (int i = 0; i < 256; ++i) {
    j = (j + S[i] + static_cast<uint8_t>(key[i & 7])) & 0xFF;   // ← khác RC4 chuẩn
    std::swap(S[i], S[j]);
  }

  std::string out;
  out.reserve(data.size());
  int i = 0;
  j = 0;
  for (unsigned char byte : data) {
    i = (i + 1) & 0xFF;
    j = (j + S[i]) & 0xFF;
    std::swap(S[i], S[j]);
    const int keystream = S[(S[i] + S[j]) & 0xFF];
    out.push_back(static_cast<char>(keystream ^ byte));
  }
  return out;
}

}  // namespace tencent::crypto
