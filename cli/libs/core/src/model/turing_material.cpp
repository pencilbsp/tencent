// turing_material.cpp — Xem include/tencent/core/model/turing_material.h.
#include "tencent/core/model/turing_material.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>
#include <stdexcept>

#include <zlib.h>

#include "tencent/core/common/hex.h"
#include "tencent/core/common/strings.h"

namespace tencent::turing {
namespace {

constexpr uint32_t kDelta = 0x9E3779B9u;

uint32_t load_le32(const char* p) {
  return static_cast<uint32_t>(static_cast<unsigned char>(p[0])) |
         (static_cast<uint32_t>(static_cast<unsigned char>(p[1])) << 8) |
         (static_cast<uint32_t>(static_cast<unsigned char>(p[2])) << 16) |
         (static_cast<uint32_t>(static_cast<unsigned char>(p[3])) << 24);
}

void store_le32(std::string& out, uint32_t v) {
  out.push_back(static_cast<char>(v & 0xffu));
  out.push_back(static_cast<char>((v >> 8) & 0xffu));
  out.push_back(static_cast<char>((v >> 16) & 0xffu));
  out.push_back(static_cast<char>((v >> 24) & 0xffu));
}

std::vector<uint32_t> unpack_words(const std::string& data) {
  if (data.size() % 4 != 0) throw std::runtime_error("XXTEA data not aligned");
  std::vector<uint32_t> v;
  v.reserve(data.size() / 4);
  for (std::size_t i = 0; i < data.size(); i += 4) v.push_back(load_le32(data.data() + i));
  return v;
}

std::string pack_words(const std::vector<uint32_t>& v) {
  std::string out;
  out.reserve(v.size() * 4);
  for (uint32_t w : v) store_le32(out, w);
  return out;
}

uint32_t mx(uint32_t z, uint32_t y, uint32_t sum,
            const std::array<uint32_t, 4>& k, std::size_t p, uint32_t e) {
  return ((((z >> 5) ^ (y << 2)) + ((y >> 3) ^ (z << 4))) ^
          ((sum ^ y) + (k[(p & 3u) ^ e] ^ z)));
}

void xxtea_encrypt_words(std::vector<uint32_t>& v, const std::array<uint32_t, 4>& k) {
  std::size_t n = v.size();
  if (n < 2) return;
  uint32_t rounds = 6u + 52u / static_cast<uint32_t>(n);
  uint32_t sum = 0;
  uint32_t z = v[n - 1];
  while (rounds--) {
    sum += kDelta;
    uint32_t e = (sum >> 2) & 3u;
    for (std::size_t p = 0; p + 1 < n; ++p) {
      uint32_t y = v[p + 1];
      v[p] += mx(z, y, sum, k, p, e);
      z = v[p];
    }
    uint32_t y = v[0];
    v[n - 1] += mx(z, y, sum, k, n - 1, e);
    z = v[n - 1];
  }
}

void xxtea_decrypt_words(std::vector<uint32_t>& v, const std::array<uint32_t, 4>& k) {
  std::size_t n = v.size();
  if (n < 2) return;
  uint32_t rounds = 6u + 52u / static_cast<uint32_t>(n);
  uint32_t sum = rounds * kDelta;
  uint32_t y = v[0];
  while (rounds--) {
    uint32_t e = (sum >> 2) & 3u;
    for (std::size_t p = n - 1; p > 0; --p) {
      uint32_t z = v[p - 1];
      v[p] -= mx(z, y, sum, k, p, e);
      y = v[p];
    }
    uint32_t z = v[n - 1];
    v[0] -= mx(z, y, sum, k, 0, e);
    y = v[0];
    sum -= kDelta;
  }
}

std::array<uint32_t, 4> k26_key_words() {
  std::string key = util::hex_to_string(kK26XxteaKeyHex);
  auto words = unpack_words(key);
  return {words[0], words[1], words[2], words[3]};
}

std::array<uint32_t, 4> key_words(const std::string& key) {
  std::string padded = key.substr(0, 16);
  padded.resize(16, '\0');
  return {load_le32(padded.data()), load_le32(padded.data() + 4),
          load_le32(padded.data() + 8), load_le32(padded.data() + 12)};
}

std::string xxtea_encrypt_blob(const std::string& data) {
  std::string padded = data;
  std::size_t rem = padded.size() % 4;
  if (rem) padded.append(4 - rem, '\0');
  store_le32(padded, static_cast<uint32_t>(data.size()));
  auto words = unpack_words(padded);
  xxtea_encrypt_words(words, k26_key_words());
  return pack_words(words);
}

std::string xxtea_decrypt_blob(const std::string& data) {
  auto words = unpack_words(data);
  xxtea_decrypt_words(words, k26_key_words());
  std::string plain = pack_words(words);
  if (plain.size() < 8) return plain;
  uint32_t orig_len = load_le32(plain.data() + plain.size() - 4);
  if (orig_len > plain.size() - 4) throw std::runtime_error("invalid XXTEA suffix length");
  plain.resize(orig_len);
  return plain;
}

std::string creturn_decrypt(const std::string& data, const std::string& key) {
  if (data.size() < 8 || data.size() % 4 != 0) return "";
  auto words = unpack_words(data);
  xxtea_decrypt_words(words, key_words(key));
  std::string plain = pack_words(words);
  uint32_t n = load_le32(plain.data() + plain.size() - 4);
  if (n > plain.size() - 4) return "";
  plain.resize(n);
  return plain;
}

std::string zlib_compress_level6(const std::string& data) {
  uLongf cap = compressBound(data.size());
  std::string out(cap, '\0');
  int rc = ::compress2(reinterpret_cast<Bytef*>(&out[0]), &cap,
                       reinterpret_cast<const Bytef*>(data.data()), data.size(), 6);
  if (rc != Z_OK) throw std::runtime_error("zlib compress failed");
  out.resize(cap);
  return out;
}

std::string zlib_inflate(const std::string& data) {
  z_stream zs{};
  if (::inflateInit(&zs) != Z_OK) throw std::runtime_error("inflateInit failed");
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
  zs.avail_in = static_cast<uInt>(data.size());
  std::string out;
  std::array<char, 4096> buf{};
  int rc = Z_OK;
  while (rc == Z_OK) {
    zs.next_out = reinterpret_cast<Bytef*>(buf.data());
    zs.avail_out = static_cast<uInt>(buf.size());
    rc = ::inflate(&zs, Z_NO_FLUSH);
    std::size_t have = buf.size() - zs.avail_out;
    out.append(buf.data(), have);
  }
  ::inflateEnd(&zs);
  if (rc != Z_STREAM_END) throw std::runtime_error("inflate failed");
  return out;
}

bool is_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string xml_unescape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size();) {
    if (value.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 5; }
    else if (value.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 4; }
    else if (value.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 4; }
    else if (value.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 6; }
    else if (value.compare(i, 6, "&apos;") == 0) { out.push_back('\''); i += 6; }
    else out.push_back(value[i++]);
  }
  return out;
}

std::string attr(const std::string& tag, const std::string& name) {
  std::string marker = name + "=\"";
  std::size_t pos = tag.find(marker);
  if (pos == std::string::npos) return "";
  pos += marker.size();
  std::size_t end = tag.find('"', pos);
  return end == std::string::npos ? "" : xml_unescape(tag.substr(pos, end - pos));
}

bool is_k26_uuid_dashed(const std::string& s) {
  if (s.size() != 36) return false;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (s[i] != '-') return false;
    } else if (!is_hex(s[i])) {
      return false;
    }
  }
  return true;
}

std::string k26_blob_s(const std::string& data) {
  if (data.size() > 0xff) throw std::runtime_error("K26 short blob too long");
  std::string out;
  out.push_back(static_cast<char>(data.size()));
  out += data;
  return out;
}

std::pair<std::string, std::string> parse_k26_plain(const std::string& plain) {
  if (plain.size() < 3 || static_cast<unsigned char>(plain[0]) != 0x08 ||
      plain[1] != 0) {
    throw std::runtime_error("bad K26 map header");
  }
  unsigned count = static_cast<unsigned char>(plain[2]);
  std::size_t pos = 3;
  std::string uuid, aux;
  for (unsigned i = 0; i < count; ++i) {
    if (pos + 2 > plain.size() || static_cast<unsigned char>(plain[pos]) != 0x06)
      throw std::runtime_error("bad K26 key tag");
    std::size_t key_len = static_cast<unsigned char>(plain[pos + 1]);
    pos += 2;
    if (pos + key_len > plain.size()) throw std::runtime_error("truncated K26 key");
    std::string key = plain.substr(pos, key_len);
    pos += key_len;
    if (pos + 2 > plain.size() || static_cast<unsigned char>(plain[pos]) != 0x16)
      throw std::runtime_error("bad K26 value tag");
    std::size_t value_len = static_cast<unsigned char>(plain[pos + 1]);
    pos += 2;
    if (pos + value_len > plain.size()) throw std::runtime_error("truncated K26 value");
    std::string value = plain.substr(pos, value_len);
    pos += value_len;
    if (key == "1") uuid = value;
    else if (key == "2") aux = value;
  }
  if (pos != plain.size()) throw std::runtime_error("trailing K26 bytes");
  return {uuid, aux};
}

int read_jce_int(const std::string& data, std::size_t& pos, int field_type, int& value) {
  const std::array<int, 4> widths = {1, 2, 4, 8};
  if (field_type == 12) {
    value = 0;
    return 0;
  }
  if (field_type < 0 || field_type > 3) return -1;
  int width = widths[field_type];
  if (pos + static_cast<std::size_t>(width) > data.size()) return -1;
  int64_t v = 0;
  for (int i = 0; i < width; ++i) {
    v = (v << 8) | static_cast<unsigned char>(data[pos + i]);
  }
  if (data[pos] & 0x80) v -= (int64_t{1} << (width * 8));
  pos += width;
  value = static_cast<int>(v);
  return 0;
}

}  // namespace

// hex_encode/hex_decode ĐÃ CHUYỂN sang common/hex.h (util::hex_encode,
// util::hex_to_string) — trước đây file này có bản sao y hệt.

bool pref_enabled(const std::map<std::string, std::string>& prefs, const std::string& key) {
  if (key.empty()) return true;
  const auto it = prefs.find(key);
  if (it == prefs.end()) return false;
  const std::string value = util::to_lower(util::trim(it->second));
  return value == "1" || value == "true" || value == "yes" || value == "y" || value == "on";
}

std::string normalize_k26_uuid(const std::string& value) {
  std::string uuid;
  uuid.reserve(value.size());
  for (char c : value) {
    if (!std::isspace(static_cast<unsigned char>(c)))
      uuid.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  if (uuid.size() == 32) {
    uuid = uuid.substr(0, 8) + "-" + uuid.substr(8, 4) + "-" + uuid.substr(12, 4) +
           "-" + uuid.substr(16, 4) + "-" + uuid.substr(20);
  }
  if (!is_k26_uuid_dashed(uuid)) throw std::runtime_error("invalid K26 UUID");
  return uuid;
}

std::string k26_wire_from_uuid(const std::string& dashed_uuid) {
  std::string uuid = normalize_k26_uuid(dashed_uuid);
  uuid.erase(std::remove(uuid.begin(), uuid.end(), '-'), uuid.end());
  return uuid;
}

std::string build_k26_cache_plain(const std::string& dashed_uuid, const std::string& aux) {
  std::string uuid = normalize_k26_uuid(dashed_uuid);
  std::string out;
  out += std::string("\x08\x00\x02", 3);
  out.push_back('\x06');
  out += k26_blob_s("1");
  out.push_back('\x16');
  out += k26_blob_s(uuid);
  out.push_back('\x06');
  out += k26_blob_s("2");
  out.push_back('\x16');
  out += k26_blob_s(aux);
  return out;
}

std::string encode_k26_cache(const std::string& dashed_uuid, const std::string& aux) {
  return xxtea_encrypt_blob(zlib_compress_level6(build_k26_cache_plain(dashed_uuid, aux)));
}

K26Decoded decode_k26_cache(const std::string& blob) {
  K26Decoded out;
  std::string decrypted;
  try {
    decrypted = xxtea_decrypt_blob(blob);
  } catch (...) {
    out.status = -2002;
    return out;
  }
  std::string plain;
  try {
    plain = zlib_inflate(decrypted);
  } catch (...) {
    out.status = -2001;
    return out;
  }
  try {
    auto fields = parse_k26_plain(plain);
    out.uuid_dashed = normalize_k26_uuid(fields.first);
    out.aux = fields.second;
    out.status = 0;
  } catch (...) {
    out.status = -31000;
  }
  return out;
}

std::optional<std::pair<int, int>> decode_k34_cl_cache(const std::string& data) {
  std::size_t pos = 0;
  int values[3] = {0, 0, 0};
  for (int expected_tag = 0; expected_tag < 3; ++expected_tag) {
    if (pos >= data.size()) return std::nullopt;
    unsigned char head = static_cast<unsigned char>(data[pos++]);
    int field_type = head & 0x0f;
    int tag = head >> 4;
    if (tag == 15) {
      if (pos >= data.size()) return std::nullopt;
      tag = static_cast<unsigned char>(data[pos++]);
    }
    if (tag != expected_tag) return std::nullopt;
    int value = 0;
    if (read_jce_int(data, pos, field_type, value) != 0) return std::nullopt;
    values[tag] = value;
  }
  return std::make_pair(values[0], values[2]);
}

std::string decode_gooseberry_pref(const std::string& hex_value) {
  if (hex_value.empty()) return "";
  try {
    // Creturn hashes the 21-byte Gooseberry key with MD5; the result is the
    // same verified 16-byte key used by the K26 cache codec.
    static const std::string key = util::hex_to_string(kK26XxteaKeyHex);
    return creturn_decrypt(util::hex_to_string(hex_value), key);
  } catch (...) {
    return "";
  }
}

std::map<std::string, std::string> parse_turing_prefs_xml(const std::string& xml) {
  std::map<std::string, std::string> out;
  std::size_t pos = 0;
  while ((pos = xml.find('<', pos)) != std::string::npos) {
    std::size_t end = xml.find('>', pos);
    if (end == std::string::npos) break;
    std::string tag = xml.substr(pos, end - pos + 1);
    std::string name = attr(tag, "name");
    if (name.empty()) {
      pos = end + 1;
      continue;
    }
    std::string value;
    if (tag.compare(0, 7, "<string") == 0) {
      std::size_t close = xml.find("</string>", end + 1);
      if (close == std::string::npos) break;
      value = xml_unescape(xml.substr(end + 1, close - end - 1));
      std::string decoded = decode_gooseberry_pref(value);
      if (!decoded.empty()) value = decoded;
      pos = close + 9;
    } else {
      value = attr(tag, "value");
      pos = end + 1;
    }
    out[name] = value;
  }
  return out;
}

std::string build_k34_native_tail(int edb_state, int load_bitmap, int save_bitmap,
                                  const std::vector<int>& nrf,
                                  const std::vector<int>& nsf,
                                  int ud) {
  auto pick = [](const std::vector<int>& v, std::size_t i) {
    return i < v.size() ? v[i] : -1;
  };
  // Native appends the five status slots as 5,4,3,2,1.
  int order[] = {4, 3, 2, 1, 0};
  std::string out = "EDB:" + std::to_string(edb_state) + "_" + std::to_string(load_bitmap) +
                    "_" + std::to_string(save_bitmap);
  out += ",NRF:";
  for (int i = 0; i < 5; ++i) {
    if (i) out.push_back('_');
    out += std::to_string(pick(nrf, order[i]));
  }
  out += ",NSF:";
  for (int i = 0; i < 5; ++i) {
    if (i) out.push_back('_');
    out += std::to_string(pick(nsf, order[i]));
  }
  out += ",UD:" + std::to_string(ud);
  return out;
}

std::vector<std::string> k26_cache_paths(const std::string& pkg) {
  std::string base = "/data/data/" + pkg;
  return {
      "/storage/emulated/0/DCIM/.android/.android_system_config.prop",
      base + "/app_turingdfp/log/.t.log",
      "/storage/emulated/0/DCIM/.tmfs/.turing.dat",
      "/storage/emulated/0/.turing.dat",
      base + "/app_turingdfp/1/.turing.dat",
  };
}

}  // namespace tencent::turing
