// ckey_v42.cpp — Xem ckey_v42.h. Port từ ../analytics/app/encode_ckey_v42.py.
#include "tencent/core/protocol/playback/ckey_codec.h"

#include <random>
#include <stdexcept>

namespace tencent::playback {

namespace {
// Ghép u16 big-endian rồi data — tương ứng struct.pack(">H", len) + f.
void append_tlv(Bytes& out, const Bytes& f) {
  if (f.size() > 0xFFFF) throw std::invalid_argument("TLV field too long for u16 len");
  out.push_back(uint8_t(f.size() >> 8));
  out.push_back(uint8_t(f.size() & 0xFF));
  out.insert(out.end(), f.begin(), f.end());
}
void append_be32(Bytes& out, uint32_t v) {
  out.push_back(uint8_t(v >> 24));
  out.push_back(uint8_t(v >> 16));
  out.push_back(uint8_t(v >> 8));
  out.push_back(uint8_t(v));
}
}  // namespace

PadEntropy random_pad_entropy() {
  std::random_device rd;
  PadEntropy ent;
  ent.rand2[0] = uint8_t(rd() & 0xFF);
  ent.rand2[1] = uint8_t(rd() & 0xFF);
  ent.head_rand1 = uint8_t(rd() & 0xFF);
  return ent;
}

// Port của encode_ckey_v42.py:44-65 (build_plaintext).
// Raw block nằm GIỮA fields[5] và fields[6] (encode_ckey_v42.py:25-28).
Bytes build_plaintext(const std::vector<Bytes>& fields, const Bytes& preamble,
                      uint32_t raw_bitmap, const std::vector<uint32_t>& raw_ints,
                      const Bytes& trailing) {
  if (fields.size() < 26)
    throw std::invalid_argument("Need at least 26 fields");  // encode_ckey_v42.py:56-57

  Bytes head = preamble;  // 24 byte
  for (int i = 0; i < 5; ++i) append_tlv(head, fields[i]);

  // Raw block: u32BE bitmap + N × u32BE int (encode_raw_block, :38-42).
  append_be32(head, raw_bitmap);
  for (uint32_t v : raw_ints) append_be32(head, v);

  for (size_t i = 5; i < fields.size(); ++i) append_tlv(head, fields[i]);
  head.insert(head.end(), trailing.begin(), trailing.end());

  // Bọc u16BE tổng độ dài ở đầu (struct.pack(">H", len(head)) + head).
  Bytes out;
  out.reserve(head.size() + 2);
  out.push_back(uint8_t(head.size() >> 8));
  out.push_back(uint8_t(head.size() & 0xFF));
  out.insert(out.end(), head.begin(), head.end());
  return out;
}

// Port của encode_ckey_v42.py:67-77 (qqtea_pad_and_encrypt).
Bytes qqtea_pad_and_encrypt(const Bytes& length_prefixed, const Bytes& tea_key,
                            const PadEntropy& ent) {
  const Bytes& body = length_prefixed;
  // pad_count = (-2 - len(body)) % 8 — modulo DƯƠNG kiểu Python (AGENTS.md §5).
  int raw = (-2 - int(body.size())) % 8;
  int pad_count = ((raw % 8) + 8) % 8;

  // head_byte = (rand1 & 0xF8) | (pad_count & 7)  (encode_ckey_v42.py:72)
  uint8_t head_byte = uint8_t((ent.head_rand1 & 0xF8) | (pad_count & 7));

  Bytes plaintext;
  plaintext.reserve(1 + 2 + pad_count + body.size() + 7);
  plaintext.push_back(head_byte);
  plaintext.push_back(ent.rand2[0]);
  plaintext.push_back(ent.rand2[1]);
  plaintext.insert(plaintext.end(), pad_count, 0x00);
  plaintext.insert(plaintext.end(), body.begin(), body.end());
  plaintext.insert(plaintext.end(), 7, 0x00);  // b"\x00" * 7

  if (plaintext.size() % 8 != 0)
    throw std::runtime_error("qqtea plaintext not multiple of 8");  // assert :73

  Bytes cipher = crypto::qqtea_cbc_encrypt(plaintext, tea_key);
  // checksum = struct.pack(">I", crypto::poly131(length_prefixed))  (encode_ckey_v42.py:76)
  uint32_t cs = crypto::poly131(length_prefixed);
  cipher.push_back(uint8_t(cs >> 24));
  cipher.push_back(uint8_t(cs >> 16));
  cipher.push_back(uint8_t(cs >> 8));
  cipher.push_back(uint8_t(cs));
  return cipher;
}

// Port của encode_ckey_v42.py:79-92 (encode_ckey).
std::string encode_ckey(const std::vector<Bytes>& fields, const Bytes& preamble,
                        const Bytes& tea_key, const Bytes& xor_key,
                        uint32_t raw_bitmap, const std::vector<uint32_t>& raw_ints,
                        const Bytes& trailing, const PadEntropy& ent) {
  Bytes length_prefixed = build_plaintext(fields, preamble, raw_bitmap, raw_ints, trailing);
  Bytes cipher_with_checksum = qqtea_pad_and_encrypt(length_prefixed, tea_key, ent);
  Bytes xored = crypto::xor_bytes(cipher_with_checksum, xor_key);
  return "--01" + crypto::tencent_b64_encode(xored);
}

// ── DECODE ───────────────────────────────────────────────────────────────────

namespace {
uint16_t be16(const uint8_t* p) { return uint16_t((uint16_t(p[0]) << 8) | p[1]); }
uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
// popcount thủ công (C++17, không có std::popcount).
int popcount32(uint32_t v) {
  int c = 0;
  while (v) { v &= (v - 1); ++c; }
  return c;
}
}  // namespace

// Port của encode_ckey_v42.py:95-117.
void extract_fields_from_captured(const Bytes& tlv, std::vector<Bytes>& fields,
                                  uint32_t& raw_bitmap, std::vector<uint32_t>& raw_ints,
                                  Bytes& trailing) {
  fields.clear();
  raw_ints.clear();
  size_t pos = 0;

  // 5 field đầu.
  for (int i = 0; i < 5; ++i) {
    if (pos + 2 > tlv.size()) throw std::runtime_error("decode: missing TLV in the first 5 fields");
    uint16_t L = be16(&tlv[pos]);
    pos += 2;
    if (pos + L > tlv.size()) throw std::runtime_error("decode: field exceeds buffer");
    fields.emplace_back(tlv.begin() + pos, tlv.begin() + pos + L);
    pos += L;
  }

  // Raw block: u32BE bitmap + popcount(bitmap) × u32BE int.
  if (pos + 4 > tlv.size()) throw std::runtime_error("decode: missing raw bitmap");
  raw_bitmap = be32(&tlv[pos]);
  pos += 4;
  int n = popcount32(raw_bitmap);
  for (int i = 0; i < n; ++i) {
    if (pos + 4 > tlv.size()) throw std::runtime_error("decode: missing raw int");
    raw_ints.push_back(be32(&tlv[pos]));
    pos += 4;
  }

  // Các field còn lại (dừng khi không còn căn được TLV) — encode_ckey_v42.py:109-115.
  while (pos + 2 <= tlv.size()) {
    uint16_t L = be16(&tlv[pos]);
    if (pos + 2 + L > tlv.size()) break;
    pos += 2;
    fields.emplace_back(tlv.begin() + pos, tlv.begin() + pos + L);
    pos += L;
  }
  trailing.assign(tlv.begin() + pos, tlv.end());
}

// Port của parse_full_captured, encode_ckey_v42.py:120-139.
CkeyDecoded decode_ckey(const std::string& ckey_str, const Bytes& xor_key,
                        const Bytes& tea_key) {
  if (ckey_str.rfind("--01", 0) != 0)
    throw std::invalid_argument("cKey must start with --01");  // assert :122

  Bytes ct = crypto::tencent_b64_decode(ckey_str.substr(4));
  Bytes pre = crypto::xor_bytes(ct, xor_key);
  if (pre.size() < 4) throw std::runtime_error("decode: payload is too short");

  Bytes cipher(pre.begin(), pre.end() - 4);
  uint32_t stored_cs = be32(&pre[pre.size() - 4]);
  Bytes plain = crypto::qqtea_cbc_decrypt(cipher, tea_key);
  if (plain.size() < 3) throw std::runtime_error("decode: plaintext is too short");

  CkeyDecoded out;
  uint8_t head = plain[0];
  out.pad_count = head & 7;
  // Tái lập entropy để roundtrip byte-exact (xem ckey_v42.h CkeyDecoded).
  out.entropy.head_rand1 = head;
  out.entropy.rand2[0] = plain[1];
  out.entropy.rand2[1] = plain[2];

  size_t payload_start = 3 + out.pad_count;
  if (payload_start + 2 > plain.size()) throw std::runtime_error("decode: invalid pad_count");
  Bytes payload(plain.begin() + payload_start, plain.end());

  out.tlv_len = be16(&payload[0]);
  if (size_t(2 + out.tlv_len) > payload.size())
    throw std::runtime_error("decode: truncated TLV");

  // length_prefixed = payload[2:2+tlv_len] (= preamble + flat TLV).
  Bytes head_block(payload.begin() + 2, payload.begin() + 2 + out.tlv_len);
  // checksum tính trên payload[:2+tlv_len] (gồm cả 2 byte length prefix).
  Bytes cs_region(payload.begin(), payload.begin() + 2 + out.tlv_len);
  out.stored_checksum = stored_cs;
  out.computed_checksum = crypto::poly131(cs_region);

  out.preamble.assign(head_block.begin(), head_block.begin() + 24);
  Bytes flat_tlv(head_block.begin() + 24, head_block.end());
  extract_fields_from_captured(flat_tlv, out.fields, out.raw_bitmap, out.raw_ints,
                               out.trailing);
  return out;
}

}  // namespace tencent::playback
