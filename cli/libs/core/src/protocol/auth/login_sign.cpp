// login_sign.cpp — Port byte-exact của `energy` MAC. Xem login_sign.h + AGENTS.md §0.
//
// Map nguồn (libqimei.so / ../analytics/app/energy_port.py):
//   RC4            energy_port.py:85-110   (sub_116750 / 0x10df40)
//   ChaCha20 block energy_port.py:121-136  (0x10f680, 10 double-round)
//   Bionic random  energy_port.py:153-179  (srand/random TYPE_3 degree=31 sep=3)
//   derive_nonce   energy_port.py:182-190  (0x10c1e4..0x10f93c)
//   nonce keys     energy_port.py:193-213
//   nonce stream   energy_port.py:215-233  (mask trước A1 MD5)
//   message_fold   energy_port.py:293-307  (A0/A1 + folded buffer, 0x10f900..0x112f00)
//   pass_b         energy_port.py:310-319  (0x113000..0x116860)
//   assemble       energy_port.py:256-258  (layout 50 byte đã verify)
#include "tencent/core/protocol/auth/login_sign.h"

#include <array>
#include <cstring>
#include <stdexcept>

#include <zlib.h>  // crc32 — KHỚP tuyệt đối zlib.crc32 của Python (energy_port.py:246)

namespace tencent::auth {
namespace {

// ── Hằng số đã verify (IDA / unidbg) — energy_port.py:60-79 ───────────────────
// KEY16 = xmmword_1AB880.
const std::string KEY16 = std::string(
    "\xa1\x61\x87\x89\xf2\xb7\xd5\x97\x13\x7b\x6c\x58\x9c\x6e\xc3\x3c", 16);
const std::string MAGIC = std::string("\x01\x55\x04\x03", 4);
const std::string MARKER = std::string("\x01\x11", 2);
// nonce_key cho pass B — hằng số per-build (getrandom=0x41), energy_port.py:69.
const std::string NONCE_KEY_FIXED = std::string("\xc3\x3c\xa1\x61\x87\x89\xf2\xb7", 8);
// Bảng 50 ký tự cho byte nonce (energy_port.py:77).
const std::string NONCE_RAND_TABLE =
    "\"#$()123456789:>ABCDEEFHRSTabcdefghijklmnrstuvwxyz";
const std::string CHACHA_CONST = "expand 32-byte k";
const std::string CHACHA_TAIL = std::string("\x69\x6b\xc7\xcf\x29\xd5\xe6\x5c", 8);

// ── MD5 (RFC 1321) — std lib không có; tự port để byte-exact ──────────────────
// energy_port.py dùng hashlib.md5 ở A1 (line 304) và pass B (line 312).
struct Md5 {
  uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  static uint32_t rotl(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

  static std::array<uint8_t, 16> digest(const std::string& msg) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7,
                              12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9,
                              14, 20, 5, 9, 14, 20, 4, 11, 16, 23, 4, 11, 16,
                              23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    std::string m = msg;
    uint64_t bitlen = static_cast<uint64_t>(m.size()) * 8;
    m.push_back(static_cast<char>(0x80));
    while (m.size() % 64 != 56) m.push_back('\0');
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<char>((bitlen >> (8 * i)) & 0xFF));

    Md5 h;
    for (std::size_t off = 0; off < m.size(); off += 64) {
      uint32_t M[16];
      for (int i = 0; i < 16; ++i) {
        M[i] = (uint8_t)m[off + 4 * i] | ((uint8_t)m[off + 4 * i + 1] << 8) |
               ((uint8_t)m[off + 4 * i + 2] << 16) | ((uint8_t)m[off + 4 * i + 3] << 24);
      }
      uint32_t A = h.a0, B = h.b0, C = h.c0, D = h.d0;
      for (int i = 0; i < 64; ++i) {
        uint32_t F;
        int g;
        if (i < 16) { F = (B & C) | (~B & D); g = i; }
        else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
        else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
        else { F = C ^ (B | ~D); g = (7 * i) % 16; }
        F = F + A + K[i] + M[g];
        A = D; D = C; C = B;
        B = B + rotl(F, S[i]);
      }
      h.a0 += A; h.b0 += B; h.c0 += C; h.d0 += D;
    }
    std::array<uint8_t, 16> out{};
    uint32_t v[4] = {h.a0, h.b0, h.c0, h.d0};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) out[4 * i + j] = (v[i] >> (8 * j)) & 0xFF;
    return out;
  }
};

// ── ChaCha20 (energy_port.py:116-136) ────────────────────────────────────────
inline uint32_t rotl32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

void qr(uint32_t* s, int a, int b, int c, int d) {
  s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 16);
  s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 12);
  s[a] += s[b]; s[d] = rotl32(s[d] ^ s[a], 8);
  s[c] += s[d]; s[b] = rotl32(s[b] ^ s[c], 7);
}

void chacha20_block(const uint32_t in[16], uint32_t out[16]) {
  uint32_t s[16];
  std::memcpy(s, in, sizeof(s));
  for (int i = 0; i < 10; ++i) {
    qr(s, 0, 4, 8, 12); qr(s, 1, 5, 9, 13); qr(s, 2, 6, 10, 14); qr(s, 3, 7, 11, 15);
    qr(s, 0, 5, 10, 15); qr(s, 1, 6, 11, 12); qr(s, 2, 7, 8, 13); qr(s, 3, 4, 9, 14);
  }
  for (int i = 0; i < 16; ++i) out[i] = s[i] + in[i];
}

// ── Bionic random() TYPE_3 sau srand(seed) (energy_port.py:153-179) ───────────
void bionic_random_type3(uint32_t seed, int count, uint32_t* values) {
  uint32_t state[31];
  state[0] = seed;
  for (int i = 1; i < 31; ++i) {
    // x = 16807*lo - 2836*hi với phép chia/dư của prev (energy_port.py:159-164).
    uint32_t prev = state[i - 1];
    int32_t hi = static_cast<int32_t>(prev / 127773);
    int32_t lo = static_cast<int32_t>(prev % 127773);
    int32_t x = 16807 * lo - 2836 * hi;
    if (x <= 0) x += 0x7FFFFFFF;
    state[i] = static_cast<uint32_t>(x);
  }
  int fidx = 3, ridx = 0;
  auto step = [&]() -> uint32_t {
    state[fidx] = state[fidx] + state[ridx];
    uint32_t value = (state[fidx] >> 1) & 0x7FFFFFFF;
    fidx = (fidx + 1) % 31;
    ridx = (ridx + 1) % 31;
    return value;
  };
  for (int i = 0; i < 310; ++i) step();
  for (int i = 0; i < count; ++i) values[i] = step();
}

// crc16_hi: high16(zlib_crc32(buf)) → 2 byte big-endian (energy_port.py:245-251).
std::string crc16_hi_bytes(const std::string& buf) {
  uint32_t c = static_cast<uint32_t>(
      crc32(0L, reinterpret_cast<const Bytef*>(buf.data()), buf.size()));
  uint32_t hi = c >> 16;
  std::string out;
  out.push_back(static_cast<char>((hi >> 8) & 0xFF));
  out.push_back(static_cast<char>(hi & 0xFF));
  return out;
}

std::string md5_str(const std::string& msg) {
  auto d = Md5::digest(msg);
  return std::string(reinterpret_cast<const char*>(d.data()), d.size());
}

}  // namespace

// ── RC4 (energy_port.py:85-110) ──────────────────────────────────────────────
std::string rc4(const std::string& key, const std::string& data) {
  if (key.empty()) throw std::invalid_argument("rc4: empty key");
  int S[256];
  for (int i = 0; i < 256; ++i) S[i] = i;
  int j = 0;
  for (int i = 0; i < 256; ++i) {
    j = (j + S[i] + static_cast<uint8_t>(key[i & 7])) & 0xFF;
    std::swap(S[i], S[j]);
  }
  std::string out;
  out.reserve(data.size());
  int i = 0;
  j = 0;
  for (unsigned char b : data) {
    i = (i + 1) & 0xFF;
    j = (j + S[i]) & 0xFF;
    std::swap(S[i], S[j]);
    int K = S[(S[i] + S[j]) & 0xFF];
    out.push_back(static_cast<char>(K ^ b));
  }
  return out;
}

// ── derive_nonce (energy_port.py:182-190) ────────────────────────────────────
std::string derive_nonce(uint32_t usec) {
  if (usec >= 1'000'000) throw std::invalid_argument("usec must be in [0, 1000000)");
  uint32_t r[2];
  bionic_random_type3(usec, 2, r);
  int r0 = r[0] % 50, r1 = r[1] % 50;
  uint8_t c0 = static_cast<uint8_t>(NONCE_RAND_TABLE[r0]);
  uint8_t c1 = static_cast<uint8_t>(NONCE_RAND_TABLE[r1]);
  std::string out;
  out.push_back(static_cast<char>((c0 + 0x3C) & 0xFF));
  out.push_back(static_cast<char>((c1 + 0x3C) & 0xFF));
  out.push_back(static_cast<char>((c0 + 0x50) & 0xFF));
  out.push_back(static_cast<char>((c1 + 0x50) & 0xFF));
  out.push_back(static_cast<char>((usec >> 24) & 0xFF));
  out.push_back(static_cast<char>((usec >> 16) & 0xFF));
  out.push_back(static_cast<char>((usec >> 8) & 0xFF));
  out.push_back(static_cast<char>(usec & 0xFF));
  return out;
}

// ── nonce keys (energy_port.py:193-213) ──────────────────────────────────────
std::string nonce_rc4_key(const std::string& nonce) {
  if (nonce.size() != 8) throw std::invalid_argument("nonce must be 8 bytes");
  auto n = [&](int i) { return static_cast<uint8_t>(nonce[i]); };
  std::string k(8, '\0');
  k[0] = static_cast<char>(0x39 ^ n(2));
  k[1] = static_cast<char>(0x21 ^ n(3));
  k[2] = static_cast<char>(0x61);
  k[3] = static_cast<char>(0x24);
  k[4] = static_cast<char>(0x28);
  k[5] = static_cast<char>(0x65);
  k[6] = static_cast<char>(0x26 ^ n(0));
  k[7] = static_cast<char>(0x4F ^ n(1));
  return k;
}

std::string nonce_chacha_key(const std::string& nonce) {
  std::string key = nonce_rc4_key(nonce);
  return rc4(key, key);
}

// ── nonce stream ChaCha (energy_port.py:215-233) ─────────────────────────────
std::string derive_nonce_stream(const std::string& nonce, std::size_t nbytes) {
  std::string key8 = nonce_chacha_key(nonce);
  std::string out;
  uint64_t block = 0;
  while (out.size() < nbytes) {
    // state = CHACHA_CONST(16) | key8(8) | 24 zero | <II block_lo block_hi> | TAIL(8)
    std::string st;
    st += CHACHA_CONST;          // 16
    st += key8;                  // 8
    st.append(24, '\0');         // 24
    uint32_t lo = static_cast<uint32_t>(block & 0xFFFFFFFF);
    uint32_t hi = static_cast<uint32_t>(block >> 32);
    for (int i = 0; i < 4; ++i) st.push_back(static_cast<char>((lo >> (8 * i)) & 0xFF));
    for (int i = 0; i < 4; ++i) st.push_back(static_cast<char>((hi >> (8 * i)) & 0xFF));
    st += CHACHA_TAIL;           // 8  → tổng 64
    uint32_t words[16], res[16];
    for (int i = 0; i < 16; ++i) {
      words[i] = (uint8_t)st[4 * i] | ((uint8_t)st[4 * i + 1] << 8) |
                 ((uint8_t)st[4 * i + 2] << 16) | ((uint8_t)st[4 * i + 3] << 24);
    }
    chacha20_block(words, res);
    for (int i = 0; i < 16; ++i)
      for (int b = 0; b < 4; ++b) out.push_back(static_cast<char>((res[i] >> (8 * b)) & 0xFF));
    ++block;
  }
  out.resize(nbytes);
  return out;
}

// ── outside_in_fold (energy_port.py:264-273) ─────────────────────────────────
std::string outside_in_fold(const std::string& src) {
  std::size_t n = src.size();
  std::string out(n, '\0');
  std::size_t half = n / 2;
  for (std::size_t i = 0; i < half; ++i) {
    out[2 * i] = src[i];
    out[2 * i + 1] = src[n - 1 - i];
  }
  if (n & 1) out[n - 1] = src[half];
  return out;
}

// ── energy MAC (energy_port.py:322-337) ──────────────────────────────────────
std::string energy(const std::string& message, uint32_t usec) {
  std::string nonce = derive_nonce(usec);

  // message_fold (energy_port.py:293-307):
  std::string stream = derive_nonce_stream(nonce, message.size() + 4);
  // A1 = MD5((message || nonce[4:8]) xor stream)[:8]
  std::string plain = message + nonce.substr(4, 4);
  std::string xored(plain.size(), '\0');
  for (std::size_t i = 0; i < plain.size(); ++i)
    xored[i] = static_cast<char>(static_cast<uint8_t>(plain[i]) ^ static_cast<uint8_t>(stream[i]));
  std::string a1 = md5_str(xored).substr(0, 8);
  std::string a0 = crc16_hi_bytes(nonce + a1);
  std::string folded = outside_in_fold(message + KEY16);

  // pass_b (energy_port.py:310-314): B[0:16] = MD5(folded) xor (RC4(nk,nk) || 0*8)
  std::string digest = md5_str(folded);
  std::string mask = rc4(NONCE_KEY_FIXED, NONCE_KEY_FIXED) + std::string(8, '\0');
  std::string b16(16, '\0');
  for (int i = 0; i < 16; ++i)
    b16[i] = static_cast<char>(static_cast<uint8_t>(digest[i]) ^ static_cast<uint8_t>(mask[i]));
  std::string b18 = b16 + crc16_hi_bytes(b16);

  // assemble (energy_port.py:256-258): 01550403|a0|nonce|a1|0111|b18|00*8
  return MAGIC + a0 + nonce + a1 + MARKER + b18 + std::string(8, '\0');
}

std::string make_sign(const std::string& message, uint32_t usec) {
  static const char* hexd = "0123456789abcdef";
  std::string e = energy(message, usec);
  std::string out = "0002";
  for (unsigned char c : e) { out.push_back(hexd[c >> 4]); out.push_back(hexd[c & 0xF]); }
  return out;
}

std::string build_original_data(const std::string& guid, const std::string& vurefresh,
                                uint64_t now_ms, uint32_t rnd) {
  return "g=" + guid + "&v=" + vurefresh + "&t=" + std::to_string(now_ms) +
         "&r=" + std::to_string(rnd);
}

}  // namespace tencent::auth
