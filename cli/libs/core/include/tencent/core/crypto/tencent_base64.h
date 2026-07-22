// tencent_base64.h — Biến thể Base64 riêng của Tencent.
//
// Port 1:1 của ../analytics/app/parse_ckey_v42.py:58-78.
// Là Base64 CHUẨN (không phải url-safe) rồi hoán hai ký tự cuối bảng và bỏ
// padding:  '+' → '_' ,  '/' → '-' ,  bỏ hết '='.
//
// Phải pass test byte-exact (tests/crypto/test_crypto.cpp) — xem AGENTS.md §0.
#pragma once

#include <string>

#include "tencent/core/common/bytes.h"

namespace tencent::crypto {

std::string tencent_b64_encode(const Bytes& data);
Bytes tencent_b64_decode(const std::string& text);

}  // namespace tencent::crypto
