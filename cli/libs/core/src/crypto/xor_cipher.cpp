// xor_cipher.cpp — Xem include/tencent/core/crypto/xor_cipher.h.
#include "tencent/core/crypto/xor_cipher.h"

#include <stdexcept>

namespace tencent::crypto {

Bytes xor_bytes(const Bytes& data, const Bytes& key) {
  if (key.empty()) throw std::invalid_argument("xor_bytes: khoá không được rỗng");
  Bytes out(data.size());
  for (std::size_t i = 0; i < data.size(); ++i) {
    out[i] = data[i] ^ key[i % key.size()];
  }
  return out;
}

}  // namespace tencent::crypto
