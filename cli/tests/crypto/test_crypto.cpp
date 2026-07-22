// test_crypto.cpp — Test BYTE-EXACT cho Pha 1 (tencent_crypto + ckey_v42).
//
// So khớp từng byte với ground-truth sinh từ Python gốc + capture thật
// (tests/generated_vectors.h, sinh bởi tests/gen_vectors.py). Xem AGENTS.md §0.
// Bất kỳ mismatch nào -> exit != 0 -> ctest đỏ -> KHÔNG được commit.
#include <cstdio>
#include <string>
#include <vector>

#include "tencent/core/protocol/playback/ckey_codec.h"
#include "vectors/generated_vectors.h"
#include "tencent/core/common/hex.h"

using tencent::Bytes;
using tencent::util::hex_encode;
using tencent::util::hex_to_bytes;


static int g_failures = 0;

static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}

static Bytes HB(const char* hex) { return hex_to_bytes(std::string(hex)); }

int main() {
  std::printf("== Pha 1: crypto + ckey byte-exact ==\n");

  // ── poly131 ────────────────────────────────────────────────────────────
  check(tencent::crypto::poly131(HB(vec::POLY131_IN_HEX)) == vec::POLY131_OUT, "poly131");

  // ── TencentBase64 encode (cả hai nhánh padding) ────────────────────────
  check(tencent::crypto::tencent_b64_encode(HB(vec::B64_IN_HEX)) == std::string(vec::B64_OUT),
        "tencent_b64_encode rem==2");
  check(tencent::crypto::tencent_b64_encode(HB(vec::B64_IN2_HEX)) == std::string(vec::B64_OUT2),
        "tencent_b64_encode rem==1");

  // ── TencentBase64 decode (roundtrip phải khớp input) ───────────────────
  check(tencent::crypto::tencent_b64_decode(std::string(vec::B64_OUT)) == HB(vec::B64_IN_HEX),
        "tencent_b64_decode rem==2");
  check(tencent::crypto::tencent_b64_decode(std::string(vec::B64_OUT2)) == HB(vec::B64_IN2_HEX),
        "tencent_b64_decode rem==1");

  // ── XOR lặp khóa ───────────────────────────────────────────────────────
  check(tencent::crypto::xor_bytes(HB(vec::XOR_IN_HEX), HB(vec::XOR_KEY_HEX)) == HB(vec::XOR_OUT_HEX),
        "xor_bytes");

  // ── TEA block đơn ──────────────────────────────────────────────────────
  check(tencent::crypto::tea_encrypt_block(HB(vec::TEA_BLOCK_IN_HEX), HB(vec::TEA_KEY_HEX)) ==
            HB(vec::TEA_BLOCK_ENC_HEX),
        "tea_encrypt_block");
  check(tencent::crypto::tea_decrypt_block(HB(vec::TEA_BLOCK_IN_HEX), HB(vec::TEA_KEY_HEX)) ==
            HB(vec::TEA_BLOCK_DEC_HEX),
        "tea_decrypt_block");

  // ── QQ-TEA CBC dual-state ──────────────────────────────────────────────
  check(tencent::crypto::qqtea_cbc_encrypt(HB(vec::CBC_IN_HEX), HB(vec::TEA_KEY_HEX)) ==
            HB(vec::CBC_ENC_HEX),
        "qqtea_cbc_encrypt");
  check(tencent::crypto::qqtea_cbc_decrypt(HB(vec::CBC_IN_HEX), HB(vec::TEA_KEY_HEX)) ==
            HB(vec::CBC_DEC_HEX),
        "qqtea_cbc_decrypt");
  // CBC roundtrip (enc -> dec == input)
  check(tencent::crypto::qqtea_cbc_decrypt(
            tencent::crypto::qqtea_cbc_encrypt(HB(vec::CBC_IN_HEX), HB(vec::TEA_KEY_HEX)),
            HB(vec::TEA_KEY_HEX)) == HB(vec::CBC_IN_HEX),
        "qqtea_cbc roundtrip");

  // ── cKey v4.2 đầy đủ (test quan trọng nhất) ────────────────────────────
  std::vector<Bytes> fields;
  for (const auto& fh : vec::CKEY_FIELDS_HEX) fields.push_back(HB(fh.c_str()));

  tencent::playback::PadEntropy ent;
  Bytes r2 = HB(vec::CKEY_RAND2_HEX);
  ent.rand2[0] = r2[0];
  ent.rand2[1] = r2[1];
  ent.head_rand1 = vec::CKEY_HEAD_RAND1;

  std::vector<uint32_t> raw_ints(vec::CKEY_RAW_INTS.begin(), vec::CKEY_RAW_INTS.end());
  std::string ckey = tencent::playback::encode_ckey(
      fields, HB(vec::CKEY_PREAMBLE_HEX), HB(vec::CKEY_TEA_KEY_HEX), HB(vec::CKEY_XOR_KEY_HEX),
      vec::CKEY_RAW_BITMAP, raw_ints, HB(vec::CKEY_TRAILING_HEX), ent);

  bool ckey_ok = (ckey == std::string(vec::CKEY_EXPECTED));
  check(ckey_ok, "encode_ckey byte-exact vs Python");
  if (!ckey_ok) {
    std::printf("    expected: %s\n", vec::CKEY_EXPECTED);
    std::printf("    actual:   %s\n", ckey.c_str());
  }

  // ── DECODE: decode_ckey(CAP_CKEY) khớp Python parse_full_captured ───────
  tencent::playback::CkeyDecoded dec = tencent::playback::decode_ckey(
      std::string(vec::CAP_CKEY), HB(vec::CKEY_XOR_KEY_HEX), HB(vec::CKEY_TEA_KEY_HEX));

  check(dec.preamble == HB(vec::CAP_PREAMBLE_HEX), "decode preamble");

  std::vector<Bytes> cap_fields;
  for (const auto& fh : vec::CAP_FIELDS_HEX) cap_fields.push_back(HB(fh.c_str()));
  check(dec.fields == cap_fields, "decode fields");

  check(dec.raw_bitmap == vec::CAP_RAW_BITMAP, "decode raw_bitmap");
  std::vector<uint32_t> cap_ints(vec::CAP_RAW_INTS.begin(), vec::CAP_RAW_INTS.end());
  check(dec.raw_ints == cap_ints, "decode raw_ints");
  check(dec.trailing == HB(vec::CAP_TRAILING_HEX), "decode trailing");
  check(dec.pad_count == vec::CAP_PAD_COUNT, "decode pad_count");
  check(dec.tlv_len == vec::CAP_TLV_LEN, "decode tlv_len");
  check(dec.stored_checksum == vec::CAP_STORED_CS, "decode stored_checksum");
  check(dec.computed_checksum == vec::CAP_COMPUTED_CS, "decode computed_checksum");
  check(dec.stored_checksum == dec.computed_checksum, "decode checksum stored==computed");

  // ── ROUNDTRIP: decode(CKEY_EXPECTED) → encode == CKEY_EXPECTED ──────────
  // CKEY_EXPECTED do encoder của ta sinh (pad=0) nên encode∘decode = identity.
  tencent::playback::CkeyDecoded rt = tencent::playback::decode_ckey(
      std::string(vec::CKEY_EXPECTED), HB(vec::CKEY_XOR_KEY_HEX), HB(vec::CKEY_TEA_KEY_HEX));
  check(rt.fields == fields, "roundtrip decode fields == template");
  std::string re_ckey = tencent::playback::encode_ckey(
      rt.fields, rt.preamble, HB(vec::CKEY_TEA_KEY_HEX), HB(vec::CKEY_XOR_KEY_HEX),
      rt.raw_bitmap, rt.raw_ints, rt.trailing, rt.entropy);
  check(re_ckey == std::string(vec::CKEY_EXPECTED), "roundtrip encode(decode()) byte-exact");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
