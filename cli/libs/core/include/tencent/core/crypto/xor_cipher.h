// xor_cipher.h — XOR với khoá lặp vòng.
//
// Port 1:1 của ../analytics/app/parse_ckey_v42.py:85-87. Đây là lớp ngoài cùng
// của cKey v4.2 (sau checksum, trước Base64).
#pragma once

#include "tencent/core/common/bytes.h"

namespace tencent::crypto {

// out[i] = data[i] ^ key[i % key.size()]. Ném std::invalid_argument nếu khoá rỗng.
Bytes xor_bytes(const Bytes& data, const Bytes& key);

}  // namespace tencent::crypto
