// pb_codec.cpp — Port byte-exact của ../analytics/app/pb_codec.py. Xem pb_codec.h.
#include "tencent/core/codec/protobuf.h"

#include <stdexcept>

namespace tencent::pb {

// ../analytics/app/pb_codec.py:28-39
std::string write_varint(uint64_t n) {
  std::string out;
  while (true) {
    uint8_t b = n & 0x7F;
    n >>= 7;
    if (n) {
      out.push_back(static_cast<char>(b | 0x80));
    } else {
      out.push_back(static_cast<char>(b));
      break;
    }
  }
  return out;
}

// ../analytics/app/pb_codec.py:15-25
uint64_t read_varint(const std::string& b, std::size_t& o) {
  int shift = 0;
  uint64_t result = 0;
  while (true) {
    if (o >= b.size()) throw std::runtime_error("read_varint: out of data");
    uint8_t x = static_cast<uint8_t>(b[o]);
    ++o;
    result |= static_cast<uint64_t>(x & 0x7F) << shift;
    if (!(x & 0x80)) break;
    shift += 7;
  }
  return result;
}

// ../analytics/app/pb_codec.py:48-58
std::string Field::encode() const {
  std::string tag = write_varint((static_cast<uint64_t>(num) << 3) | static_cast<uint64_t>(wire));
  if (wire == 0) return tag + write_varint(intval);
  if (wire == 1) {
    std::string out = tag;
    uint64_t v = intval;
    for (int i = 0; i < 8; ++i) { out.push_back(static_cast<char>(v & 0xFF)); v >>= 8; }
    return out;
  }
  if (wire == 5) {
    std::string out = tag;
    uint32_t v = static_cast<uint32_t>(intval);
    for (int i = 0; i < 4; ++i) { out.push_back(static_cast<char>(v & 0xFF)); v >>= 8; }
    return out;
  }
  if (wire == 2) return tag + write_varint(data.size()) + data;
  throw std::runtime_error("bad wire");
}

// ../analytics/app/pb_codec.py:61-80
std::vector<Field> parse(const std::string& b) {
  std::vector<Field> fields;
  std::size_t o = 0;
  const std::size_t n = b.size();
  while (o < n) {
    uint64_t tag = read_varint(b, o);
    int num = static_cast<int>(tag >> 3);
    int wire = static_cast<int>(tag & 7);
    if (wire == 0) {
      uint64_t v = read_varint(b, o);
      fields.push_back(Field::varint(num, v));
    } else if (wire == 1) {
      if (o + 8 > n) throw std::runtime_error("parse i64: out of data");
      uint64_t v = 0;
      for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(static_cast<uint8_t>(b[o + i])) << (8 * i);
      o += 8;
      fields.push_back(Field::i64(num, v));
    } else if (wire == 5) {
      if (o + 4 > n) throw std::runtime_error("parse i32: out of data");
      uint64_t v = 0;
      for (int i = 0; i < 4; ++i) v |= static_cast<uint64_t>(static_cast<uint8_t>(b[o + i])) << (8 * i);
      o += 4;
      fields.push_back(Field::i32(num, v));
    } else if (wire == 2) {
      uint64_t ln = read_varint(b, o);
      if (o + ln > n) throw std::runtime_error("parse len: out of data");
      fields.push_back(Field::bytes(num, b.substr(o, ln)));
      o += ln;
    } else {
      throw std::runtime_error("unsupported wire");
    }
  }
  return fields;
}

// ../analytics/app/pb_codec.py:83-84
std::string serialize(const std::vector<Field>& fields) {
  std::string out;
  for (const auto& f : fields) out += f.encode();
  return out;
}

// ../analytics/app/pb_codec.py:87-91
const Field* get_first(const std::vector<Field>& fields, int num) {
  for (const auto& f : fields)
    if (f.num == num) return &f;
  return nullptr;
}

// ../analytics/app/pb_codec.py:94-95
std::vector<const Field*> get_all(const std::vector<Field>& fields, int num) {
  std::vector<const Field*> out;
  for (const auto& f : fields)
    if (f.num == num) out.push_back(&f);
  return out;
}

}  // namespace tencent::pb
