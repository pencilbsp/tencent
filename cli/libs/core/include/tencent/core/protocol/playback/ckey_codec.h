// ckey_v42.h — Bộ mã hóa cKey v4.2 cho getvinfo (offline).
//
// Port 1:1 từ ../analytics/app/encode_ckey_v42.py (đã pass live: em=0/exem=0, xem
// ../analytics/app/getvinfo_param_provenance.md).
//
// Pipeline (encode_ckey_v42.py:1-13):
//   TLV fields (32) → 24-byte preamble || [u16BE total_len][TLV có length-prefix]
//   → pad: [head_byte(low3=pad_count)][rand(2)][\x00 * pad_count][payload][\x00*7]
//   → QQ-TEA CBC encrypt với tea_key
//   → append uint32_BE(crypto::poly131(length_prefixed_payload))
//   → XOR lặp khóa với xor_key
//   → TencentBase64
//   → tiền tố "--01"
//
// Native tham chiếu: libtvkplayer.so tvk_vinfo_add_ckey_metrics @ 0x4EB4CC
// (chèn cKey vào body). Khóa tea/xor hardcode trong libqimei.so (DUMP, không port).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tencent/core/common/bytes.h"
#include "tencent/core/crypto/poly131.h"
#include "tencent/core/crypto/qqtea.h"
#include "tencent/core/crypto/tencent_base64.h"
#include "tencent/core/crypto/xor_cipher.h"

namespace tencent::playback {

// Entropy được INJECT để test byte-exact (AGENTS.md §0/§5). Trong Python,
// các byte này lấy từ os.urandom (encode_ckey_v42.py:71-72). Mặc định runtime
// nên dùng nguồn ngẫu nhiên thật; test truyền giá trị tất định.
struct PadEntropy {
  std::array<uint8_t, 2> rand2{};  // os.urandom(2)  — encode_ckey_v42.py:71
  uint8_t head_rand1 = 0;          // os.urandom(1)[0] — encode_ckey_v42.py:72
};

// Sinh PadEntropy ngẫu nhiên thật (dùng cho runtime, KHÔNG dùng trong test).
PadEntropy random_pad_entropy();

// build_plaintext — port của encode_ckey_v42.py:44-65.
// Ghép preamble (24B) + 5 field đầu (u16BE len + data) + raw block
// (u32BE bitmap + N×u32BE int) + các field còn lại + trailing, rồi bọc bằng
// u16BE tổng độ dài ở đầu.
Bytes build_plaintext(const std::vector<Bytes>& fields, const Bytes& preamble,
                      uint32_t raw_bitmap, const std::vector<uint32_t>& raw_ints,
                      const Bytes& trailing);

// qqtea_pad_and_encrypt — port của encode_ckey_v42.py:67-77.
// pad_count = (-2 - len(body)) % 8  (modulo dương kiểu Python).
Bytes qqtea_pad_and_encrypt(const Bytes& length_prefixed, const Bytes& tea_key,
                            const PadEntropy& ent);

// encode_ckey — port của encode_ckey_v42.py:79-92. Trả về chuỗi "--01...".
std::string encode_ckey(const std::vector<Bytes>& fields, const Bytes& preamble,
                        const Bytes& tea_key, const Bytes& xor_key,
                        uint32_t raw_bitmap, const std::vector<uint32_t>& raw_ints,
                        const Bytes& trailing, const PadEntropy& ent);

// ── DECODE (đảo của encode) ──────────────────────────────────────────────────

// Kết quả giải mã một cKey đã capture.
// `entropy` được tái lập từ plaintext (head_rand1 = plain[0], rand2 = plain[1:3]),
// nhờ đó encode lại bằng entropy này cho ra ĐÚNG cKey gốc (roundtrip byte-exact).
struct CkeyDecoded {
  Bytes preamble;                  // 24 byte
  std::vector<Bytes> fields;       // 5 field đầu + (sau raw block) các field còn lại
  uint32_t raw_bitmap = 0;
  std::vector<uint32_t> raw_ints;  // popcount(raw_bitmap) phần tử
  Bytes trailing;
  PadEntropy entropy;
  // meta (encode_ckey_v42.py:135)
  int pad_count = 0;
  uint16_t tlv_len = 0;
  uint32_t stored_checksum = 0;    // poly131 lưu trong cKey
  uint32_t computed_checksum = 0;  // poly131 tính lại — phải khớp stored
};

// extract_fields_from_captured — port của encode_ckey_v42.py:95-117.
// Tách flat TLV (sau preamble) thành 5 field đầu + raw block + các field còn lại
// + trailing (phần dư không căn được theo TLV chặt chẽ).
void extract_fields_from_captured(const Bytes& tlv, std::vector<Bytes>& fields,
                                  uint32_t& raw_bitmap, std::vector<uint32_t>& raw_ints,
                                  Bytes& trailing);

// decode_ckey — port của parse_full_captured, encode_ckey_v42.py:120-139.
// KHÔNG ném lỗi khi checksum lệch (giống Python) — chỉ ghi vào meta để caller kiểm.
CkeyDecoded decode_ckey(const std::string& ckey_str, const Bytes& xor_key,
                        const Bytes& tea_key);

}  // namespace tencent::playback
