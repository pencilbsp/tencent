// bionic_random.h — Tái lập random() của Bionic libc (Android).
//
// Port của ../analytics/app/energy_port.py:153-179.
//
// Native gọi srand(tv_usec) rồi random() vài lần để lấy byte nonce. Muốn ra
// đúng chuỗi byte đó thì phải mô phỏng ĐÚNG bộ sinh của Android: TYPE_3
// (bậc 31, separation 3), khởi tạo bằng LCG Lehmer 16807 và bỏ 310 giá trị đầu.
// rand() của thư viện chuẩn trên máy chủ cho kết quả KHÁC HẲN.
#pragma once

#include <cstdint>

namespace tencent::crypto {

// Sinh `count` giá trị đầu tiên sau srand(seed) vào mảng `values`.
void bionic_random_type3(uint32_t seed, int count, uint32_t* values);

}  // namespace tencent::crypto
