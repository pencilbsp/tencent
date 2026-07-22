// poly131.cpp — Xem include/tencent/core/crypto/poly131.h.
#include "tencent/core/crypto/poly131.h"

namespace tencent::crypto {

uint32_t poly131(const Bytes& data) {
  uint32_t hash = 0;  // uint32_t tự wrap mod 2^32, tương đương `& 0xFFFFFFFF`.
  for (uint8_t byte : data) hash = uint32_t(byte) + 131u * hash;
  return hash & 0x7FFF'FFFFu;
}

}  // namespace tencent::crypto
