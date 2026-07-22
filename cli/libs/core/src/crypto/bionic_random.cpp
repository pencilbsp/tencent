// bionic_random.cpp — Xem include/tencent/core/crypto/bionic_random.h.
// Thân hàm giữ NGUYÊN từ bản đã verify byte-exact (src/login_sign.cpp cũ).
#include "tencent/core/crypto/bionic_random.h"

namespace tencent::crypto {

void bionic_random_type3(uint32_t seed, int count, uint32_t* values) {
  uint32_t state[31];
  state[0] = seed;
  for (int i = 1; i < 31; ++i) {
    // x = 16807*lo - 2836*hi, với hi/lo là thương/dư của phần tử trước chia
    // 127773 (energy_port.py:159-164) — chính là LCG Lehmer của libc.
    const uint32_t previous = state[i - 1];
    const int32_t hi = static_cast<int32_t>(previous / 127773);
    const int32_t lo = static_cast<int32_t>(previous % 127773);
    int32_t x = 16807 * lo - 2836 * hi;
    if (x <= 0) x += 0x7FFFFFFF;
    state[i] = static_cast<uint32_t>(x);
  }

  int front = 3, rear = 0;   // separation = 3 của TYPE_3
  auto step = [&]() -> uint32_t {
    state[front] = state[front] + state[rear];
    const uint32_t value = (state[front] >> 1) & 0x7FFFFFFF;
    front = (front + 1) % 31;
    rear = (rear + 1) % 31;
    return value;
  };

  for (int i = 0; i < 310; ++i) step();   // Bionic bỏ 310 giá trị đầu
  for (int i = 0; i < count; ++i) values[i] = step();
}

}  // namespace tencent::crypto
