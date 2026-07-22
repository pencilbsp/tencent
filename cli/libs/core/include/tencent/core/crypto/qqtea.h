// qqtea.h — QQ-TEA: biến thể TEA của Tencent (reverse từ libqimei.so).
//
// Port 1:1 của ../analytics/app/parse_ckey_v42.py:94-199.
//
// Khác TEA chuẩn ở hai điểm, đừng thay bằng thư viện có sẵn:
//   • Từ 32-bit đọc theo BIG-ENDIAN, cộng tràn mod 2^32 (dùng uint32_t để tự
//     wrap; `>>` là dịch phải LOGIC vì toán hạng unsigned) — AGENTS.md §5.
//   • Chế độ CBC "dual-state": mỗi khối nhớ CẢ khối trung gian T lẫn khối mã C.
//       mã hoá:  T_n = P_n ^ C_{n-1};  E_n = TEA(T_n);      C_n = E_n ^ T_{n-1}
//       giải mã: E_n = C_n ^ T_{n-1};  T_n = TEA⁻¹(E_n);    P_n = T_n ^ C_{n-1}
//     T_0 = C_0 = 8 byte 0x00.
#pragma once

#include <cstdint>

#include "tencent/core/common/bytes.h"

namespace tencent::crypto {

// Hằng số vòng (parse_ckey_v42.py:94-95).
constexpr uint32_t kTeaDelta = 0x9E3779B9u;

// Một khối 8 byte, khoá 16 byte. Ném std::invalid_argument nếu sai kích thước.
Bytes tea_encrypt_block(const Bytes& block, const Bytes& key);
Bytes tea_decrypt_block(const Bytes& block, const Bytes& key);

// Độ dài đầu vào phải là bội số của 8 (việc chèn pad do ckey_codec lo).
Bytes qqtea_cbc_encrypt(const Bytes& plaintext, const Bytes& key);
Bytes qqtea_cbc_decrypt(const Bytes& ciphertext, const Bytes& key);

}  // namespace tencent::crypto
