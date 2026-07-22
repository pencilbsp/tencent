// chacha20.cpp — Xem include/tencent/core/crypto/chacha20.h.
// Thân hàm giữ NGUYÊN từ bản đã verify byte-exact (src/login_sign.cpp cũ).
#include "tencent/core/crypto/chacha20.h"

#include <cstring>

namespace tencent::crypto {
namespace {

inline uint32_t rotl32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

// Quarter-round chuẩn ChaCha.
void quarter_round(uint32_t* s, int a, int b, int c, int d) {
  s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 16);
  s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 12);
  s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 8);
  s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 7);
}

}  // namespace

void chacha20_block(const uint32_t in[16], uint32_t out[16]) {
  uint32_t state[16];
  std::memcpy(state, in, sizeof(state));
  for (int i = 0; i < 10; ++i) {           // 10 double-round = 20 vòng
    quarter_round(state, 0, 4, 8, 12);
    quarter_round(state, 1, 5, 9, 13);
    quarter_round(state, 2, 6, 10, 14);
    quarter_round(state, 3, 7, 11, 15);
    quarter_round(state, 0, 5, 10, 15);
    quarter_round(state, 1, 6, 11, 12);
    quarter_round(state, 2, 7, 8, 13);
    quarter_round(state, 3, 4, 9, 14);
  }
  for (int i = 0; i < 16; ++i) out[i] = state[i] + in[i];
}

}  // namespace tencent::crypto
