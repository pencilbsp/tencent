// crc16.cpp — Xem include/tencent/core/crypto/crc16.h.
#include "tencent/core/crypto/crc16.h"

#include <cstdint>

#include <zlib.h>  // crc32 — KHỚP tuyệt đối zlib.crc32 của Python

namespace tencent::crypto {

std::string crc16_high_bytes(const std::string& data) {
  const uint32_t checksum = static_cast<uint32_t>(
      ::crc32(0L, reinterpret_cast<const Bytef*>(data.data()),
              static_cast<uInt>(data.size())));
  const uint32_t high = checksum >> 16;
  std::string out;
  out.push_back(static_cast<char>((high >> 8) & 0xFF));
  out.push_back(static_cast<char>(high & 0xFF));
  return out;
}

}  // namespace tencent::crypto
