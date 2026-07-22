// chacha20.h — Hàm nén ChaCha20 (10 double-round).
//
// Port của ../analytics/app/energy_port.py:116-136 (libqimei.so @ 0x10F680).
//
// CẢNH BÁO: đây KHÔNG phải API mã hoá ChaCha20 hoàn chỉnh (không có counter,
// nonce 96-bit, không tự sinh keystream theo RFC 8439). Native tự dựng khối
// trạng thái 16 từ theo cách riêng rồi gọi thẳng hàm nén — nên chỗ này chỉ
// phơi ra đúng hàm nén. Người gọi (auth/login_sign.cpp) chịu trách nhiệm dựng
// trạng thái đầu vào.
#pragma once

#include <cstdint>

namespace tencent::crypto {

// in/out là 16 từ 32-bit. Cho phép out == in? KHÔNG — hàm đọc `in` sau vòng lặp
// để cộng trạng thái ban đầu, nên hai vùng phải TÁCH BIỆT.
void chacha20_block(const uint32_t in[16], uint32_t out[16]);

}  // namespace tencent::crypto
