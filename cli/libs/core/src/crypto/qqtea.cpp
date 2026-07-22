// qqtea.cpp — Xem include/tencent/core/crypto/qqtea.h.
// Thân hàm giữ NGUYÊN từ bản đã verify byte-exact (src/tencent_crypto.cpp cũ).
#include "tencent/core/crypto/qqtea.h"

#include <stdexcept>

namespace tencent::crypto {
namespace {

uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

void put_be32(Bytes& out, uint32_t value) {
  out.push_back(uint8_t(value >> 24));
  out.push_back(uint8_t(value >> 16));
  out.push_back(uint8_t(value >> 8));
  out.push_back(uint8_t(value));
}

Bytes xor8(const Bytes& a, const Bytes& b) {
  Bytes out(8);
  for (int i = 0; i < 8; ++i) out[i] = a[i] ^ b[i];
  return out;
}

}  // namespace

// Port của parse_ckey_v42.py:132-150. uint32_t tự wrap nên không cần MASK32.
Bytes tea_encrypt_block(const Bytes& block, const Bytes& key) {
  if (block.size() != 8 || key.size() != 16) {
    throw std::invalid_argument("tea_encrypt_block: block=8, key=16");
  }
  uint32_t y = be32(&block[0]), z = be32(&block[4]);
  const uint32_t k0 = be32(&key[0]), k1 = be32(&key[4]);
  const uint32_t k2 = be32(&key[8]), k3 = be32(&key[12]);
  uint32_t s = kTeaDelta;
  for (int i = 0; i < 16; ++i) {
    y += ((k0 + (z << 4)) ^ (z + s) ^ (k1 + (z >> 5)));
    z += ((k2 + (y << 4)) ^ (s + y) ^ (k3 + (y >> 5)));
    s += kTeaDelta;
  }
  Bytes out;
  out.reserve(8);
  put_be32(out, y);
  put_be32(out, z);
  return out;
}

// Port của parse_ckey_v42.py:106-129. s khởi tạo = DELTA*16 (mod 2^32).
Bytes tea_decrypt_block(const Bytes& block, const Bytes& key) {
  if (block.size() != 8 || key.size() != 16) {
    throw std::invalid_argument("tea_decrypt_block: block=8, key=16");
  }
  uint32_t y = be32(&block[0]), z = be32(&block[4]);
  const uint32_t k0 = be32(&key[0]), k1 = be32(&key[4]);
  const uint32_t k2 = be32(&key[8]), k3 = be32(&key[12]);
  uint32_t s = kTeaDelta * 16u;  // (kTeaDelta * 16) & 0xFFFFFFFF
  for (int i = 0; i < 16; ++i) {
    z -= ((k2 + (y << 4)) ^ (s + y) ^ (k3 + (y >> 5)));
    y -= ((k0 + (z << 4)) ^ (z + s) ^ (k1 + (z >> 5)));
    s -= kTeaDelta;
  }
  Bytes out;
  out.reserve(8);
  put_be32(out, y);
  put_be32(out, z);
  return out;
}

// Port của parse_ckey_v42.py:181-199.
Bytes qqtea_cbc_encrypt(const Bytes& plaintext, const Bytes& key) {
  if (plaintext.size() % 8 != 0) {
    throw std::invalid_argument("qqtea_cbc_encrypt: độ dài phải là bội số của 8");
  }
  Bytes out;
  out.reserve(plaintext.size());
  Bytes t_prev(8, 0), c_prev(8, 0);
  for (std::size_t pos = 0; pos < plaintext.size(); pos += 8) {
    const Bytes p_block(plaintext.begin() + pos, plaintext.begin() + pos + 8);
    const Bytes t_block = xor8(p_block, c_prev);
    const Bytes e_block = tea_encrypt_block(t_block, key);
    const Bytes c_block = xor8(e_block, t_prev);
    out.insert(out.end(), c_block.begin(), c_block.end());
    t_prev = t_block;
    c_prev = c_block;
  }
  return out;
}

// Port của parse_ckey_v42.py:153-178.
Bytes qqtea_cbc_decrypt(const Bytes& ciphertext, const Bytes& key) {
  if (ciphertext.size() % 8 != 0) {
    throw std::invalid_argument("qqtea_cbc_decrypt: độ dài phải là bội số của 8");
  }
  Bytes out;
  out.reserve(ciphertext.size());
  Bytes t_prev(8, 0), c_prev(8, 0);
  for (std::size_t pos = 0; pos < ciphertext.size(); pos += 8) {
    const Bytes c_block(ciphertext.begin() + pos, ciphertext.begin() + pos + 8);
    const Bytes e_block = xor8(c_block, t_prev);
    const Bytes t_block = tea_decrypt_block(e_block, key);
    const Bytes p_block = xor8(t_block, c_prev);
    out.insert(out.end(), p_block.begin(), p_block.end());
    t_prev = t_block;
    c_prev = c_block;
  }
  return out;
}

}  // namespace tencent::crypto
