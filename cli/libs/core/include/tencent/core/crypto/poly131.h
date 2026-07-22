// poly131.h — Checksum 31-bit dùng trong cKey v4.2.
//
// Port 1:1 của ../analytics/app/parse_ckey_v42.py:98-103:
//   h = 0;  với mỗi byte b:  h = (b + 131*h) mod 2^32;  trả h & 0x7FFFFFFF
//
// Không phải hàm băm mật mã — chỉ để server phát hiện cKey hỏng.
#pragma once

#include <cstdint>

#include "tencent/core/common/bytes.h"

namespace tencent::crypto {

uint32_t poly131(const Bytes& data);

}  // namespace tencent::crypto
