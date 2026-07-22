// ckey_fields.cpp — See ckey_fields.h.
#include "tencent/core/protocol/playback/ckey_fields.h"

#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "tencent/core/protocol/playback/constants.h"
#include "tencent/core/common/hex.h"

namespace tencent::playback {

namespace {

constexpr std::array<uint32_t, 4> CHACHA_CONST_WORDS = {
    0x61707865u, 0x3320646Eu, 0x79622D32u, 0x6B206574u};
constexpr uint8_t FIELD19_BASE_STREAM_0_25[] = {
    0x01, 0x2B, 0x71, 0x74, 0x74, 0x05, 0x75, 0x74, 0x77, 0x0C, 0x79, 0x7B, 0x79,
    0x7B, 0x7D, 0x78, 0x78, 0x11, 0x66, 0x15, 0x64, 0x66, 0x65, 0x11, 0x67, 0x1F};

uint32_t rotl32(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }
uint32_t ror32(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}
void put_le32(uint8_t* p, uint32_t v) {
  p[0] = uint8_t(v);
  p[1] = uint8_t(v >> 8);
  p[2] = uint8_t(v >> 16);
  p[3] = uint8_t(v >> 24);
}

// Small RFC1321 MD5 implementation used by Field[19]. This is intentionally
// local to avoid pulling non-byte-exact external dependencies.
Bytes md5_digest(const Bytes& data) {
  static constexpr uint32_t S[64] = {
      7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
      5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
      4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
      6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  static constexpr uint32_t K[64] = {
      0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu, 0xf57c0fafu, 0x4787c62au,
      0xa8304613u, 0xfd469501u, 0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
      0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u, 0xf61e2562u, 0xc040b340u,
      0x265e5a51u, 0xe9b6c7aau, 0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
      0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu, 0xa9e3e905u, 0xfcefa3f8u,
      0x676f02d9u, 0x8d2a4c8au, 0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
      0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u, 0x289b7ec6u, 0xeaa127fau,
      0xd4ef3085u, 0x04881d05u, 0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
      0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u, 0x655b59c3u, 0x8f0ccc92u,
      0xffeff47du, 0x85845dd1u, 0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
      0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u};

  Bytes msg = data;
  uint64_t bit_len = uint64_t(msg.size()) * 8u;
  msg.push_back(0x80);
  while ((msg.size() % 64) != 56) msg.push_back(0);
  for (int i = 0; i < 8; ++i) msg.push_back(uint8_t(bit_len >> (8 * i)));

  uint32_t a0 = 0x67452301u, b0 = 0xefcdab89u, c0 = 0x98badcfeu, d0 = 0x10325476u;
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t M[16];
    for (int i = 0; i < 16; ++i) M[i] = le32(&msg[off + i * 4]);
    uint32_t A = a0, B = b0, C = c0, D = d0;
    for (int i = 0; i < 64; ++i) {
      uint32_t F = 0, g = 0;
      if (i < 16) {
        F = (B & C) | ((~B) & D);
        g = i;
      } else if (i < 32) {
        F = (D & B) | ((~D) & C);
        g = (5 * i + 1) & 15;
      } else if (i < 48) {
        F = B ^ C ^ D;
        g = (3 * i + 5) & 15;
      } else {
        F = C ^ (B | (~D));
        g = (7 * i) & 15;
      }
      uint32_t tmp = D;
      D = C;
      C = B;
      B = B + rotl32(A + F + K[i] + M[g], S[i]);
      A = tmp;
    }
    a0 += A;
    b0 += B;
    c0 += C;
    d0 += D;
  }
  Bytes out(16);
  put_le32(&out[0], a0);
  put_le32(&out[4], b0);
  put_le32(&out[8], c0);
  put_le32(&out[12], d0);
  return out;
}

uint8_t nibbleswap8(uint8_t x) { return uint8_t((x << 4) | (x >> 4)); }
uint8_t bitreverse8(uint8_t x) {
  uint8_t out = 0;
  for (int i = 0; i < 8; ++i) out = uint8_t((out << 1) | ((x >> i) & 1));
  return out;
}

Bytes derive_arx_state64(uint32_t micros) {
  Bytes out(64);
  uint32_t words[16] = {
      CHACHA_CONST_WORDS[0], CHACHA_CONST_WORDS[1], CHACHA_CONST_WORDS[2],
      CHACHA_CONST_WORDS[3], micros, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4AFE2313u,
      0x1359724Cu};
  for (int i = 0; i < 16; ++i) put_le32(&out[i * 4], words[i]);
  return out;
}

Bytes derive_custom_arx_out64(const Bytes& state64) {
  if (state64.size() != 64) throw std::invalid_argument("state64 must be 64 bytes");
  uint32_t s[16];
  for (int i = 0; i < 16; ++i) s[i] = le32(&state64[i * 4]);

  uint32_t w6 = s[0], w4 = s[1], w5 = s[2], w3 = s[3];
  uint32_t w12 = s[4], w8 = s[5], w10 = s[6], w11 = s[7];
  uint32_t w13 = s[8], w14 = s[9], w17 = s[10], w1 = s[11];
  uint32_t w16 = s[12], w15 = s[13], w2 = s[14], w0 = s[15];

  for (int round = 0; round < 10; ++round) {
    w6 = w12 + w6; w4 = w8 + w4; w5 = w10 + w5; w3 = w11 + w3;
    w16 ^= w6; w15 ^= w4; w2 ^= w5; w0 ^= w3;

    uint32_t t7 = ror32(w16, 16), t23 = ror32(w15, 16), t24 = ror32(w2, 16), t26 = ror32(w0, 16);
    w13 = t7 + w13; w14 = t23 + w14; w17 = t24 + w17; w1 = t26 + w1;
    w12 ^= w13; w8 ^= w14; w10 ^= w17; w11 ^= w1;

    t7 = ror32(w12, 20); t23 = ror32(w8, 20); t24 = ror32(w10, 20); t26 = ror32(w11, 20);
    w6 = t7 + w6; w4 = t23 + w4; w5 = t24 + w5; w3 = t26 + w3;
    w16 = w6 ^ ror32(w16, 16); w15 = w4 ^ ror32(w15, 16); w2 = w5 ^ ror32(w2, 16); w0 = w3 ^ ror32(w0, 16);

    t7 = ror32(w16, 24); t23 = ror32(w15, 24); t24 = ror32(w2, 24); t26 = ror32(w0, 24);
    w13 = t7 + w13; w14 = t23 + w14; w17 = t24 + w17; w1 = t26 + w1;
    w12 = w13 ^ ror32(w12, 20); w8 = w14 ^ ror32(w8, 20); w10 = w17 ^ ror32(w10, 20); w11 = w1 ^ ror32(w11, 20);

    t7 = ror32(w12, 25); t23 = ror32(w8, 25); t24 = ror32(w10, 25); t26 = ror32(w11, 25);
    w6 = t23 + w6; w4 = t24 + w4; w5 = t26 + w5; w3 = t7 + w3;
    w0 = w6 ^ ror32(w0, 24); w16 = w4 ^ ror32(w16, 24); w15 = w5 ^ ror32(w15, 24); w2 = w3 ^ ror32(w2, 24);

    t7 = ror32(w0, 16); t23 = ror32(w16, 16); t24 = ror32(w15, 16); t26 = ror32(w2, 16);
    w17 = t7 + w17; w1 = t23 + w1; w13 = w13 + t24; w14 = t26 + w14;
    w8 = w17 ^ ror32(w8, 25); w10 = w1 ^ ror32(w10, 25); w11 = w13 ^ ror32(w11, 25); w12 = w14 ^ ror32(w12, 25);

    t7 = ror32(w8, 20); t23 = ror32(w10, 20); t24 = ror32(w11, 20); t26 = ror32(w12, 20);
    w6 = t7 + w6; w4 = t23 + w4; w5 = t24 + w5; w3 = t26 + w3;
    w0 = w6 ^ ror32(w0, 16); w16 = w4 ^ ror32(w16, 16); w15 = w5 ^ ror32(w15, 16); w2 = w3 ^ ror32(w2, 16);

    w0 = ror32(w0, 24); w16 = ror32(w16, 24); w15 = ror32(w15, 24); w2 = ror32(w2, 24);
    w17 = w0 + w17; w1 = w16 + w1; w13 = w15 + w13; w14 = w2 + w14;
    w8 = ror32(w17 ^ ror32(w8, 20), 25);
    w10 = ror32(w1 ^ ror32(w10, 20), 25);
    w11 = ror32(w13 ^ ror32(w11, 20), 25);
    w12 = ror32(w14 ^ ror32(w12, 20), 25);
  }

  uint32_t words[16] = {
      w6 + CHACHA_CONST_WORDS[0], w4 + CHACHA_CONST_WORDS[1],
      w5 + CHACHA_CONST_WORDS[2], w3 + CHACHA_CONST_WORDS[3],
      w12 + s[4], w8 + s[5], w10 + s[6], w11 + s[7],
      w13, w14, w17, w1, w16, w15, w2, w0};
  for (int i = 8; i < 16; ++i) words[i] += s[i];

  Bytes out(64);
  for (int i = 0; i < 16; ++i) put_le32(&out[i * 4], words[i]);
  return out;
}

Bytes derive_key16_data(const Bytes& md5, const Bytes& arx_state64) {
  Bytes out64 = derive_custom_arx_out64(arx_state64);
  uint32_t md5_w21 = le32(&md5[0]);
  uint8_t first = uint8_t((le32(&out64[0]) ^ md5_w21) & 0xFF);
  Bytes key16(16);
  key16[0] = first;
  for (int i = 1; i < 16; ++i) key16[i] = md5[i] ^ out64[i];
  return key16;
}

Bytes derive_var78b0(const Bytes& md5, const Bytes& key16) {
  return Bytes{md5[0], md5[4], md5[8], md5[15], 0, 0, 0, 0,
               key16[1], key16[5], key16[9], key16[14], 0x48, 0x47, 0x46, 0x45};
}

Bytes derive_seed8(uint32_t md5_w20, uint8_t w19) {
  return Bytes{0x18, w19, 0x47, 0x2B, uint8_t((md5_w20 >> 8) & 0xFF), 0x2C, 0x4C, 0x4B};
}

Bytes derive_var80_xor_stream(const Bytes& seed8) {
  Bytes out(16);
  for (int i = 0; i < 16; ++i) out[i] = uint8_t(seed8[i & 7] + i);
  return out;
}

Bytes derive_xor_key1_stream() {
  Bytes stream(std::begin(FIELD19_BASE_STREAM_0_25), std::end(FIELD19_BASE_STREAM_0_25));
  if (stream[0]) {
    uint8_t fixed = stream[1];
    for (int i = 0; i < 24; ++i) stream[2 + i] ^= uint8_t((i + 1) ^ fixed ^ 0x18);
  }
  return stream;
}

Bytes derive_selected_src_plain() {
  Bytes stream = derive_xor_key1_stream();
  std::string encoded(stream.begin() + 2, stream.begin() + 26);
  return util::hex_to_bytes(encoded);
}

Bytes derive_selected_src(const Bytes& seed8, const Bytes& plain) {
  std::array<uint8_t, 256> sbox{};
  for (int i = 0; i < 256; ++i) sbox[i] = uint8_t(i);
  uint8_t j = 0;
  for (int i = 0; i < 256; ++i) {
    j = uint8_t(j + sbox[i] + seed8[i & 7]);
    std::swap(sbox[i], sbox[j]);
  }
  Bytes out;
  out.reserve(plain.size());
  uint8_t i = 0;
  j = 0;
  for (uint8_t b : plain) {
    i = uint8_t(i + 1);
    j = uint8_t(j + sbox[i]);
    std::swap(sbox[i], sbox[j]);
    out.push_back(uint8_t(b ^ sbox[uint8_t(sbox[i] + sbox[j])]));
  }
  return out;
}

Bytes derive_var80c0(const Bytes& stream16, const Bytes& selected_src) {
  Bytes out(16);
  for (int i = 0; i < 16; ++i) out[i] = stream16[i] ^ selected_src[i % selected_src.size()];
  return out;
}

std::tuple<uint8_t, Bytes> derive_check_tail_from_seed16(const Bytes& seed16) {
  std::array<uint8_t, 32> stream{};
  uint8_t prev = seed16[0];
  for (int i = 0; i < 15; ++i) {
    uint8_t nxt = seed16[i + 1];
    uint8_t a = nxt ^ nibbleswap8(prev);
    stream[2 * i] = a;
    stream[2 * i + 1] = bitreverse8(a) ^ 0x1A;
    prev = nxt;
  }
  uint8_t check = uint8_t(~stream[1]);
  uint8_t last = uint8_t(~(stream[1] ^ nibbleswap8(seed16[15])));
  stream[30] = last;
  stream[31] = bitreverse8(last) ^ 0x1A;
  Bytes tail(15);
  for (int i = 0; i < 15; ++i) tail[i] = stream[3 + 2 * i];
  return {check, tail};
}

Bytes assemble_field19(uint32_t md5_w20, uint8_t w19, uint32_t micros, uint8_t check,
                       const Bytes& tail15) {
  if (tail15.size() != 15) throw std::invalid_argument("Field[19] tail must be 15 bytes");
  uint16_t header = uint16_t((md5_w20 & 0xFF00u) | w19);
  Bytes out;
  out.reserve(26);
  Bytes magic = util::hex_to_bytes("4c4b8501");
  out.insert(out.end(), magic.begin(), magic.end());
  out.push_back(uint8_t(header >> 8));
  out.push_back(uint8_t(header));
  out.push_back(uint8_t(micros >> 24));
  out.push_back(uint8_t(micros >> 16));
  out.push_back(uint8_t(micros >> 8));
  out.push_back(uint8_t(micros));
  out.push_back(check);
  out.insert(out.end(), tail15.begin(), tail15.end());
  return out;
}

}  // namespace

Bytes build_field15_json_native() {
  const std::string s =
      "{\"ea\":3,\"sdrp\":1,\"sessionID\":0,\"spa\":1,\"spenclv\":2,\"ssk\":\"" +
      std::string(constants::kField15Ssk) + "\"}";
  return Bytes(s.begin(), s.end());
}

std::vector<Bytes> build_ckey_base_fields(const std::string& app_ver,
                                          const std::string& guid,
                                          const std::string& qimei36,
                                          const std::string& device_model) {
  auto b = [](const std::string& s) { return Bytes(s.begin(), s.end()); };
  std::vector<Bytes> fields = {
      b(constants::kCkeyField1Magic), b(""), b(app_ver), b(""), b(guid),
      b(constants::kQqlivePackageName), b(constants::kQqlivePackageName),
      b(constants::kQqliveCertHash), b(constants::kCkeyNullText),
      b(constants::kQqlivePackageName), b(constants::kQqliveCertHash),
      b(constants::kCkeyNullText), b(""), b(constants::kCkeyRuntimeVersion),
      build_field15_json_native(), b("{}"), b(qimei36), b(qimei36),
      Bytes(26, 0), b("00000000000000000000000000000"), b("0000000000000000"),
      b(app_ver), b(""), b(""), b(device_model), b(constants::kCkeyDeviceSdkVersion),
      b(""), b(""), b(""), b(""), b(""), b(""), b(""), b(""), b(""), b(""),
      b(""), Bytes{0x00}, b(""), b(""), b("")};
  if (fields.size() != 41) throw std::runtime_error("cKey base field count must be 41");
  return fields;
}

std::string build_field16_dd(uint64_t timestamp_ms, const Bytes& random8) {
  if (random8.size() != 8) throw std::invalid_argument("Field[16].dd random must be 8 bytes");
  std::string prefix = util::hex_encode(random8) + std::to_string(timestamp_ms) + "0";
  if (prefix.size() != 30) throw std::runtime_error("Field[16].dd prefix must be 30 chars");
  uint32_t crc = 0xFFFF'FFFFu;
  for (uint8_t b : Bytes(prefix.begin(), prefix.end())) {
    uint32_t c = (crc ^ b) & 0xFFu;
    for (int i = 0; i < 8; ++i) c = (c & 1) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
    crc = c ^ (crc >> 8);
  }
  uint16_t v = uint16_t((crc ^ 0x5432u) & 0xFFFFu);
  Bytes tail{uint8_t(v), uint8_t(v >> 8)};
  return prefix + util::hex_encode(tail);
}

Bytes build_field16_crypto_algo_json(const std::string& sd, const std::string& dd) {
  std::string s =
      "{\"bat\":\"" +
      std::string(constants::kField16Bat) + "\",\"cfls\":[\"" +
      constants::kField16AlgoId + "\"],\"cp\":\"" + constants::kField16AlgoId +
      "\",\"csal\":[\"" + constants::kField16Csal0 + "\",\"" +
      constants::kField16Csal1 + "\",\"" + constants::kField16Csal2 + "\",\"" +
      constants::kField16Csal3 + "\",\"" + constants::kField16Csal4 +
      "\"],\"csfld\":\"" + constants::kField16AlgoId + "\",\"cste\":\"" +
      constants::kField16Cste +
      "\","
      "\"dd\":\"" +
      dd + "\",\"lv\":\"" + constants::kField16Lv + "\",\"sd\":\"" + sd + "\"}";
  return Bytes(s.begin(), s.end());
}

Bytes derive_field19_from_components(uint64_t epoch_seconds, const std::string& component2,
                                     const std::string& qimei36, uint32_t micros) {
  std::string src = std::to_string(epoch_seconds) + "," + component2 + "," + qimei36;
  Bytes src_bytes(src.begin(), src.end());
  Bytes md5 = md5_digest(src_bytes);
  uint32_t md5_w20 = le32(&md5[4]);
  Bytes arx_state64 = derive_arx_state64(micros);
  Bytes key16 = derive_key16_data(md5, arx_state64);
  uint8_t w19 = key16[7];
  Bytes var78b0 = derive_var78b0(md5, key16);
  Bytes seed8 = derive_seed8(md5_w20, w19);
  Bytes selected_src = derive_selected_src(seed8, derive_selected_src_plain());
  Bytes var80c0 = derive_var80c0(derive_var80_xor_stream(seed8), selected_src);
  Bytes seed16(16);
  for (int i = 0; i < 16; ++i) seed16[i] = var78b0[i] ^ var80c0[i];
  auto [check, tail] = derive_check_tail_from_seed16(seed16);
  return assemble_field19(md5_w20, w19, micros, check, tail);
}

}  // namespace tencent::playback
