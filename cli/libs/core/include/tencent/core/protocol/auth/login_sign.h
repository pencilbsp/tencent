// login_sign.h — Port byte-exact của `energy` MAC (sign login-refresh vusession).
//
// Nguồn: ../analytics/app/energy_port.py (port của libqimei.so `sub_10C08C`, energy core =
// libqimei.so + 0x10C08C; JNI = +0x118D34). Tài liệu kiến trúc: ../analytics/app/login_sign.py
// và ../analytics/app/login_refresh_flow.md §"sign_info".
//
// sign = "0002" + hex(energy(original_data)), trong đó
//   original_data = "g=<guid>&v=<vurefresh>&t=<now_ms>&r=<rnd>"
// (../analytics/app/login_sign.py:70-94, build_original_data + signature_v2_str).
//
// Mọi hàm PHẢI pass test byte-exact (tests/test_login_sign.cpp) so với vector
// sinh từ Python (tests/gen_vectors_sign.py). Xem AGENTS.md §0.
//
// Random/clock được INJECT qua tham số (usec, now_ms, rnd) để test tất định —
// đúng kỷ luật AGENTS.md §0 ("tách phần tất định, inject random/clock").
#pragma once

#include <cstdint>
#include <string>

namespace tencent::auth {

// ── Primitive RC4 khóa 8-byte tuần hoàn (energy_port.py:85-110) ───────────────
// KSA dùng key[i & 7]; PRGA chuẩn. data có thể rỗng.
std::string rc4(const std::string& key, const std::string& data);

// ── Nonce derivation (energy_port.py:182-233) ────────────────────────────────
// Dựng nonce 8-byte từ tv_usec: 4 byte từ 2 lần Bionic random()%50 (srand(usec))
// + 4 byte big-endian = usec. usec phải nằm trong [0, 1_000_000).
std::string derive_nonce(uint32_t usec);
// Khóa RC4 8-byte dựng ngay trước pass nonce đầu (energy_port.py:193-206).
std::string nonce_rc4_key(const std::string& nonce);
// rc4(key, key) — 8 byte đầu của khóa ChaCha (energy_port.py:209-213).
std::string nonce_chacha_key(const std::string& nonce);
// Stream ChaCha20 (10 double-round) dùng mask `message||nonce[4:8]` (energy_port.py:215-233).
std::string derive_nonce_stream(const std::string& nonce, std::size_t nbytes);

// ── Biến đổi keyed theo message (energy_port.py:264-319) ──────────────────────
// Fold xen kẽ: out = src[0], src[-1], src[1], src[-2]... (energy_port.py:264-273).
std::string outside_in_fold(const std::string& src);

// ── energy MAC đầy đủ (energy_port.py:322-341) ───────────────────────────────
// Trả 50 byte: 01550403 | a0[2] | nonce[8] | a1[8] | 0111 | b18[18] | 00*8.
// nonce dẫn xuất từ `usec` (mặc định Python = micro giây hiện tại; ở đây BẮT
// BUỘC truyền để test tất định). nonce_key cho pass B = NONCE_KEY_FIXED (hằng số
// per-build, getrandom=0x41 — xem energy_port.py:69).
std::string energy(const std::string& message, uint32_t usec);

// "0002" + lowerhex(energy(message, usec)) (energy_port.py:340-341, login_sign.py).
std::string make_sign(const std::string& message, uint32_t usec);

// originalData cho sign: "g=<guid>&v=<vurefresh>&t=<now_ms>&r=<rnd>"
// (../analytics/app/login_sign.py:70-75, build_original_data).
std::string build_original_data(const std::string& guid, const std::string& vurefresh,
                                uint64_t now_ms, uint32_t rnd);

}  // namespace tencent::auth
