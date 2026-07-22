// qimei_client.cpp - Port of analytics/app/qimei_client.py and qimei_profile.py.
#include "tencent/core/protocol/identity/qimei_client.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "tencent/core/net/http_client.h"
#include "tencent/core/common/json.h"
#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/common/bytes.h"
#include "tencent/core/common/hex.h"

namespace tencent::qimei {
namespace {

using json = nlohmann::ordered_json;

constexpr char kUrl[] = "https://snowflake.qq.com/ola/v2";
constexpr char kAppKey[] = "00000VAFHI3U915P";
constexpr char kSdkVersion[] = "8.1.2.160.4";
constexpr char kPackage[] = "com.tencent.qqlive";
constexpr char kBuildId[] = "20563";
constexpr char kInnerKeyHex[] = "ff802003f67f55be8b535a97b01b459d";
constexpr char kInnerIvHex[] = "b8221969c27a8019d533b139ade71bcc";
constexpr char kRsaPublicKey[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEApLEpcsiHA/PE4MKl7WS7\n"
    "r11RijhytmEQH8jaY3nKhvq/KC29yEpJVAu+/asRp9mpoxCAiaTxJynVZMw5amwe\n"
    "24ff0Eau/MLCcfG9jFq8fp0LJwXmKtJuwha2hVr6zhW1+Q1h8f3T9hu+QMZTRxqQ\n"
    "nIY1mjqDY1/ARAK2hMzS+uoB+COosppWgFxOcQhzkjcM7Oa0RO+rSaatWsgBrKsp\n"
    "VEXFMYfHS+awD8qGXZyyOVaiDNvXgLfPecJerKCOFdujwR3isgtEEe8yyRryUm1F\n"
    "ZgdIi8e+t23Lp0XDDB6LCox7DWLKmRjQ5K/5R7+HmSwNLANkw6yTUuLtYcoMY2nO\n"
    "UwIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

constexpr std::array<uint32_t, 8> kH14Iv = {
    0x6A09A669, 0xBA67AE87, 0x7B6BF372, 0xA56CF53A,
    0x511E527F, 0x5B25688C, 0x1F73C9AB, 0x3BD0BD19};
constexpr std::array<uint32_t, 64> kShaK = {
    0x428A2F98,0x71374491,0xB5C0FBCF,0xE9B5DBA5,0x3956C25B,0x59F111F1,0x923F82A4,0xAB1C5ED5,
    0xD807AA98,0x12835B01,0x243185BE,0x550C7DC3,0x72BE5D74,0x80DEB1FE,0x9BDC06A7,0xC19BF174,
    0xE49B69C1,0xEFBE4786,0x0FC19DC6,0x240CA1CC,0x2DE92C6F,0x4A7484AA,0x5CB0A9DC,0x76F988DA,
    0x983E5152,0xA831C66D,0xB00327C8,0xBF597FC7,0xC6E00BF3,0xD5A79147,0x06CA6351,0x14292967,
    0x27B70A85,0x2E1B2138,0x4D2C6DFC,0x53380D13,0x650A7354,0x766A0ABB,0x81C2C92E,0x92722C85,
    0xA2BFE8A1,0xA81A664B,0xC24B8B70,0xC76C51A3,0xD192E819,0xD6990624,0xF40E3585,0x106AA070,
    0x19A4C116,0x1E376C08,0x2748774C,0x34B0BCB5,0x391C0CB3,0x4ED8AA4A,0x5B9CCA4F,0x682E6FF3,
    0x748F82EE,0x78A5636F,0x84C87814,0x8CC70208,0x90BEFFFA,0xA4506CEB,0xBEF9A3F7,0xC67178F2};

uint32_t ror(uint32_t v, int n) { return (v >> n) | (v << (32 - n)); }

Bytes bytes(const std::string& value) {
  return Bytes(value.begin(), value.end());
}

std::string string_bytes(const Bytes& value) {
  return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

Bytes random_bytes(std::size_t size) {
  Bytes out(size);
  if (RAND_bytes(out.data(), static_cast<int>(out.size())) != 1)
    throw std::runtime_error("Qimei RAND_bytes failed");
  return out;
}

std::string standard_b64(const Bytes& input) {
  std::string out(4 * ((input.size() + 2) / 3), '\0');
  int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                          input.data(), static_cast<int>(input.size()));
  if (n < 0) throw std::runtime_error("Qimei base64 encode failed");
  out.resize(static_cast<std::size_t>(n));
  return out;
}

Bytes standard_b64_decode(const std::string& input) {
  if (input.empty()) return {};
  Bytes out(3 * ((input.size() + 3) / 4));
  int n = EVP_DecodeBlock(out.data(),
                          reinterpret_cast<const unsigned char*>(input.data()),
                          static_cast<int>(input.size()));
  if (n < 0) throw std::runtime_error("Qimei base64 decode failed");
  std::size_t padding = 0;
  if (!input.empty() && input.back() == '=') ++padding;
  if (input.size() > 1 && input[input.size() - 2] == '=') ++padding;
  out.resize(static_cast<std::size_t>(n) - padding);
  return out;
}

Bytes digest(const EVP_MD* algorithm, const std::string& input) {
  Bytes out(static_cast<std::size_t>(EVP_MD_size(algorithm)));
  unsigned int out_len = 0;
  if (EVP_Digest(input.data(), input.size(), out.data(), &out_len, algorithm, nullptr) != 1)
    throw std::runtime_error("Qimei digest failed");
  out.resize(out_len);
  return out;
}

Bytes aes_cbc(const Bytes& input, const Bytes& key, const Bytes& iv, bool encrypt) {
  EVP_CIPHER_CTX* raw = EVP_CIPHER_CTX_new();
  if (!raw) throw std::runtime_error("Qimei EVP_CIPHER_CTX_new failed");
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(raw, EVP_CIPHER_CTX_free);
  int ok = encrypt
      ? EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, key.data(), iv.data())
      : EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_cbc(), nullptr, key.data(), iv.data());
  if (ok != 1) throw std::runtime_error("Qimei AES init failed");
  Bytes out(input.size() + 16);
  int first = 0, last = 0;
  ok = encrypt
      ? EVP_EncryptUpdate(ctx.get(), out.data(), &first, input.data(), static_cast<int>(input.size()))
      : EVP_DecryptUpdate(ctx.get(), out.data(), &first, input.data(), static_cast<int>(input.size()));
  if (ok != 1) throw std::runtime_error("Qimei AES update failed");
  ok = encrypt
      ? EVP_EncryptFinal_ex(ctx.get(), out.data() + first, &last)
      : EVP_DecryptFinal_ex(ctx.get(), out.data() + first, &last);
  if (ok != 1) throw std::runtime_error("Qimei AES final failed");
  out.resize(static_cast<std::size_t>(first + last));
  return out;
}

Bytes rsa_wrap(const Bytes& plaintext) {
  BIO* bio = BIO_new_mem_buf(kRsaPublicKey, -1);
  if (!bio) throw std::runtime_error("Qimei BIO_new_mem_buf failed");
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_ptr(bio, BIO_free);
  EVP_PKEY* key = PEM_read_bio_PUBKEY(bio_ptr.get(), nullptr, nullptr, nullptr);
  if (!key) throw std::runtime_error("Qimei PEM_read_bio_PUBKEY failed");
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key_ptr(key, EVP_PKEY_free);
  EVP_PKEY_CTX* raw = EVP_PKEY_CTX_new(key_ptr.get(), nullptr);
  if (!raw) throw std::runtime_error("Qimei EVP_PKEY_CTX_new failed");
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(raw, EVP_PKEY_CTX_free);
  if (EVP_PKEY_encrypt_init(ctx.get()) <= 0 ||
      EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) <= 0)
    throw std::runtime_error("Qimei RSA init failed");
  std::size_t size = 0;
  if (EVP_PKEY_encrypt(ctx.get(), nullptr, &size, plaintext.data(), plaintext.size()) <= 0)
    throw std::runtime_error("Qimei RSA size failed");
  Bytes out(size);
  if (EVP_PKEY_encrypt(ctx.get(), out.data(), &size, plaintext.data(), plaintext.size()) <= 0)
    throw std::runtime_error("Qimei RSA encrypt failed");
  out.resize(size);
  return out;
}

void append_varint(std::string& out, uint64_t value) {
  while (value > 0x7f) {
    out.push_back(static_cast<char>((value & 0x7f) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<char>(value));
}

void pb_int(std::string& out, int field, uint64_t value) {
  append_varint(out, static_cast<uint64_t>(field << 3));
  append_varint(out, value);
}

void pb_string(std::string& out, int field, const std::string& value) {
  append_varint(out, static_cast<uint64_t>((field << 3) | 2));
  append_varint(out, value.size());
  out += value;
}

void pb_msg(std::string& out, int field, const std::string& value) {
  pb_string(out, field, value);
}

uint64_t read_varint(const Bytes& data, std::size_t& off) {
  uint64_t value = 0;
  int shift = 0;
  while (off < data.size() && shift <= 63) {
    uint8_t b = data[off++];
    value |= static_cast<uint64_t>(b & 0x7f) << shift;
    if ((b & 0x80) == 0) return value;
    shift += 7;
  }
  throw std::runtime_error("Qimei protobuf varint truncated");
}

std::string protobuf_string(const Bytes& data, int wanted) {
  std::size_t off = 0;
  while (off < data.size()) {
    uint64_t tag = read_varint(data, off);
    int field = static_cast<int>(tag >> 3);
    int wire = static_cast<int>(tag & 7);
    if (wire == 0) {
      read_varint(data, off);
    } else if (wire == 2) {
      std::size_t len = static_cast<std::size_t>(read_varint(data, off));
      if (off + len > data.size()) throw std::runtime_error("Qimei protobuf bytes truncated");
      if (field == wanted)
        return std::string(reinterpret_cast<const char*>(data.data() + off), len);
      off += len;
    } else {
      throw std::runtime_error("Qimei protobuf wire type unsupported");
    }
  }
  return "";
}

std::array<uint8_t, 32> h14(const Bytes& input) {
  Bytes data = input;
  uint64_t bit_len = static_cast<uint64_t>(data.size()) * 8;
  data.push_back(0x80);
  while (data.size() % 64 != 56) data.push_back(0);
  for (int i = 7; i >= 0; --i) data.push_back(static_cast<uint8_t>(bit_len >> (i * 8)));
  std::array<uint32_t, 8> h = kH14Iv;
  for (std::size_t base = 0; base < data.size(); base += 64) {
    std::array<uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
      std::size_t p = base + static_cast<std::size_t>(i * 4);
      w[i] = (uint32_t(data[p]) << 24) | (uint32_t(data[p + 1]) << 16) |
             (uint32_t(data[p + 2]) << 8) | data[p + 3];
    }
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t s1 = ror(e,6) ^ ror(e,11) ^ ror(e,25);
      uint32_t ch = (e & f) ^ ((~e) & g);
      uint32_t t1 = hh + s1 + ch + kShaK[i] + w[i];
      uint32_t s0 = ror(a,2) ^ ror(a,13) ^ ror(a,22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = s0 + maj;
      hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    std::array<uint32_t,8> next={a,b,c,d,e,f,g,hh};
    for (int i=0;i<8;++i) h[i]+=next[i];
  }
  std::array<uint8_t,32> out{};
  for (int i=0;i<8;++i) {
    out[i*4]=static_cast<uint8_t>(h[i]>>24);
    out[i*4+1]=static_cast<uint8_t>(h[i]>>16);
    out[i*4+2]=static_cast<uint8_t>(h[i]>>8);
    out[i*4+3]=static_cast<uint8_t>(h[i]);
  }
  return out;
}

std::string native_transform(const std::string& input) {
  Bytes cipher = aes_cbc(bytes(input), bytes("lvcwmSYVr2Axv1gn"), bytes("Zs0ntDqG2jyhKN0c"), true);
  return standard_b64(cipher);
}

std::string rc4_hex(const std::string& input) {
  Bytes key = bytes("ohmybid");
  std::array<uint8_t,256> s{};
  for (int i=0;i<256;++i) s[i]=static_cast<uint8_t>(i);
  int j=0;
  for (int i=0;i<256;++i) {
    j=(j+s[i]+key[static_cast<std::size_t>(i)%key.size()])&255;
    std::swap(s[i],s[j]);
  }
  Bytes out; out.reserve(input.size());
  int i=0; j=0;
  for (uint8_t b : bytes(input)) {
    i=(i+1)&255; j=(j+s[i])&255; std::swap(s[i],s[j]);
    out.push_back(b ^ s[(s[i]+s[j])&255]);
  }
  return "bi23" + util::hex_encode(out);
}

std::string rich_json(const DeviceProfile& p) {
  json j;
  j["harmony"]="0"; j["clone"]="0"; j["containe"]="";
  j["oz"]=native_transform(p.android_id); j["oz2"]=""; j["oo"]=native_transform(p.model);
  j["kelong"]="0"; j["ip"]=""; j["multiUser"]="0";
  j["bod"]=p.board; j["brd"]=p.brand; j["dv"]=p.device;
  j["firstLevel"]=p.first_api_level; j["manufact"]=p.manufacturer;
  j["name"]=p.product_name; j["host"]=p.build_host; j["kernel"]=p.kernel;
  j["pre"]="0"; j["av"]=p.app_version; j["ch"]=""; j["svr"]="";
  j["fit"]=p.first_install_ms; j["jv"]=kSdkVersion;
  return j.dump();
}

std::vector<std::string> split(const std::string& input, char delimiter) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    std::size_t end = input.find(delimiter, start);
    out.push_back(input.substr(start, end == std::string::npos
                                         ? std::string::npos : end - start));
    if (end == std::string::npos) return out;
    start = end + 1;
  }
}

int timestamp_nanos(const std::string& human) {
  std::size_t dot = human.find('.');
  if (dot == std::string::npos) return 0;
  std::string digits;
  for (std::size_t i = dot + 1; i < human.size() && digits.size() < 9; ++i) {
    if (human[i] < '0' || human[i] > '9') break;
    digits.push_back(human[i]);
  }
  if (digits.empty()) return 0;
  digits.append(9 - digits.size(), '0');
  return std::stoi(digits);
}

std::string stat_timestamp(const std::string& seconds_text,
                           const std::string& human,
                           int offset_minutes) {
  if (seconds_text.empty()) return "";
  int64_t seconds = 0;
  try {
    seconds = std::stoll(seconds_text);
  } catch (...) {
    return "";
  }
  std::time_t shifted = static_cast<std::time_t>(
      seconds + static_cast<int64_t>(offset_minutes) * 60);
  std::tm local{};
#if defined(_WIN32)
  gmtime_s(&local, &shifted);
#else
  gmtime_r(&shifted, &local);
#endif
  int absolute_offset = offset_minutes < 0 ? -offset_minutes : offset_minutes;
  char date[32];
  std::strftime(date, sizeof(date), "%Y-%m-%d%H%M%S", &local);
  char zone[8];
  std::snprintf(zone, sizeof(zone), "%c%02d%02d",
                offset_minutes < 0 ? '-' : '+',
                absolute_offset / 60, absolute_offset % 60);
  std::ostringstream out;
  out << date << '.' << std::setw(9) << std::setfill('0')
      << timestamp_nanos(human) << zone;
  return out.str();
}

std::string fsid_hex(const std::string& token) {
  if (token.empty()) return "";
  int base = 10;
  for (char c : token) {
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
      base = 16;
      break;
    }
  }
  try {
    std::size_t consumed = 0;
    uint64_t value = std::stoull(token, &consumed, base);
    if (consumed != token.size()) return token;
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
  } catch (...) {
    return token;
  }
}

enum class SignalField {
  Atime,
  Mtime,
  Inode,
  Links,
  Fsid,
  Blocks,
  Files,
};

using SignalMapping = std::pair<int, SignalField>;

const std::array<std::vector<SignalMapping>, 31> kSignalMappings = {{
    {{1,SignalField::Atime},{2,SignalField::Mtime},{11,SignalField::Inode},{12,SignalField::Links}},
    {{3,SignalField::Fsid},{5,SignalField::Blocks},{6,SignalField::Files}},
    {{4,SignalField::Fsid},{7,SignalField::Blocks},{8,SignalField::Files}},
    {{13,SignalField::Atime},{14,SignalField::Mtime},{15,SignalField::Inode},{16,SignalField::Links}},
    {{17,SignalField::Atime},{18,SignalField::Mtime},{19,SignalField::Inode},{20,SignalField::Links}},
    {{21,SignalField::Atime},{22,SignalField::Mtime},{23,SignalField::Inode},{24,SignalField::Links}},
    {{25,SignalField::Atime},{26,SignalField::Mtime},{27,SignalField::Inode},{28,SignalField::Links}},
    {{29,SignalField::Atime},{30,SignalField::Mtime},{31,SignalField::Inode},{32,SignalField::Links}},
    {{33,SignalField::Atime},{34,SignalField::Mtime},{35,SignalField::Inode},{36,SignalField::Links}},
    {{37,SignalField::Atime},{38,SignalField::Mtime},{39,SignalField::Inode},{40,SignalField::Links}},
    {{41,SignalField::Atime},{42,SignalField::Mtime},{43,SignalField::Inode},{44,SignalField::Links}},
    {{45,SignalField::Atime},{46,SignalField::Inode}},
    {{47,SignalField::Atime},{48,SignalField::Mtime}},
    {{49,SignalField::Atime},{50,SignalField::Inode}},
    {{51,SignalField::Atime},{52,SignalField::Inode}},
    {{53,SignalField::Inode}},
    {{54,SignalField::Mtime},{55,SignalField::Inode}},
    {{56,SignalField::Inode}},
    {{57,SignalField::Inode}},
    {{58,SignalField::Atime},{59,SignalField::Mtime},{60,SignalField::Inode}},
    {{61,SignalField::Inode}},
    {{62,SignalField::Atime},{63,SignalField::Inode}},
    {{64,SignalField::Atime},{65,SignalField::Inode}},
    {{66,SignalField::Inode}},
    {{67,SignalField::Atime},{68,SignalField::Inode}},
    {{69,SignalField::Inode}},
    {{70,SignalField::Inode}},
    {{71,SignalField::Mtime},{72,SignalField::Inode}},
    {{73,SignalField::Mtime}},
    {{74,SignalField::Atime}},
    {{75,SignalField::Files}},
}};

std::string stat_signal_value(const std::vector<std::string>& fields,
                              SignalField wanted,
                              int timezone_offset_minutes) {
  switch (wanted) {
    case SignalField::Atime:
      return fields.size() >= 6
          ? stat_timestamp(fields[0], fields[5], timezone_offset_minutes) : "";
    case SignalField::Mtime:
      return fields.size() >= 7
          ? stat_timestamp(fields[1], fields[6], timezone_offset_minutes) : "";
    case SignalField::Inode:
      return fields.size() >= 4 ? fields[3] : "";
    case SignalField::Links:
      return fields.size() >= 5 ? fields[4] : "";
    case SignalField::Fsid:
      return fields.empty() ? "" : fsid_hex(fields[0]);
    case SignalField::Blocks:
      return fields.size() >= 2 ? fields[1] : "";
    case SignalField::Files:
      return fields.size() >= 3 ? fields[2] : "";
  }
  return "";
}

std::string build_device_signal(const json& qimei, const std::string& boot_id,
                                int timezone_offset_minutes) {
  if (!qimei.is_object()) return "";
  if (qimei.contains("device_signal_kv") &&
      qimei["device_signal_kv"].is_string() &&
      !qimei["device_signal_kv"].get<std::string>().empty())
    return qimei["device_signal_kv"].get<std::string>();

  std::map<int, std::string> values;
  const json& records = qimei.value("stat_records", json::array());
  if (records.is_array()) {
    for (const auto& record : records) {
      if (!record.is_object()) continue;
      int index = -1;
      try {
        const auto& raw_index = record.at("index");
        index = raw_index.is_string() ? std::stoi(raw_index.get<std::string>())
                                      : raw_index.get<int>();
      } catch (...) {
        continue;
      }
      if (index < 0 || index >= static_cast<int>(kSignalMappings.size()))
        continue;
      std::string raw;
      if (record.contains("raw") && record["raw"].is_string())
        raw = record["raw"].get<std::string>();
      if (raw.empty()) continue;
      std::vector<std::string> fields = split(raw, '|');
      for (const auto& mapping : kSignalMappings[static_cast<std::size_t>(index)])
        values[mapping.first] =
            stat_signal_value(fields, mapping.second, timezone_offset_minutes);
    }
  }
  values[9] = boot_id;
  values[10] = "1";

  std::string out;
  auto append = [&](int key) {
    auto it = values.find(key);
    if (it == values.end()) return;
    if (!out.empty()) out.push_back(';');
    out += "k" + std::to_string(key) + ":" + it->second;
  };
  for (int key = 1; key <= 9; ++key) append(key);
  for (int key = 11; key <= 75; ++key) append(key);
  append(10);
  return out;
}

std::string field18(int64_t now_sec) {
  json j;
  j["0"]=now_sec; j["10"]=134; j["11"]=2097152; j["16"]=1; j["17"]=0;
  j["2"]=json::array(); j["21"]=2; j["22"]="arm64-v8a"; j["23"]=json::array();
  j["25"]=0; j["26"]=""; j["27"]=0; j["28"]=0; j["4.1"]=2;
  Bytes cipher = aes_cbc(bytes(j.dump()), bytes("ebaecfe4fe48c925"),
                         bytes("bbf35926e3321062"), true);
  return standard_b64(cipher);
}

std::string request_id() {
  Bytes data = random_bytes(14);
  return util::hex_encode(data) + "1";
}

void store_le32(uint8_t* p, uint32_t v) {
  p[0]=static_cast<uint8_t>(v); p[1]=static_cast<uint8_t>(v>>8);
  p[2]=static_cast<uint8_t>(v>>16); p[3]=static_cast<uint8_t>(v>>24);
}

std::array<uint32_t,16> sn_rounds(std::array<uint32_t,16> x) {
  auto& w11=x[0]; auto& w8=x[1]; auto& w9=x[2]; auto& w10=x[3];
  auto& w6=x[4]; auto& w4=x[5]; auto& w5=x[6]; auto& w3=x[7];
  auto& w15=x[8]; auto& w16=x[9]; auto& w2=x[10]; auto& w0=x[11];
  auto& w13=x[12]; auto& w14=x[13]; auto& w17=x[14]; auto& w1=x[15];
  for (int round=0; round<10; ++round) {
    w6+=w11; w4+=w8; w5+=w9; w3+=w10;
    w15^=w6; w16^=w4; w2^=w5; w0^=w3;
    uint32_t a=ror(w15,16),b=ror(w16,16),c=ror(w2,16),d=ror(w0,16);
    w13+=a; w14+=b; w17+=c; w1+=d;
    w11^=w13; w8^=w14; w9^=w17; w10^=w1;
    a=ror(w11,20); b=ror(w8,20); c=ror(w9,20); d=ror(w10,20);
    w6+=a; w4+=b; w5+=c; w3+=d;
    w15=w6^ror(w15,16); w16=w4^ror(w16,16); w2=w5^ror(w2,16); w0=w3^ror(w0,16);
    a=ror(w15,24); b=ror(w16,24); c=ror(w2,24); d=ror(w0,24);
    w13+=a; w14+=b; w17+=c; w1+=d;
    w11=w13^ror(w11,20); w8=w14^ror(w8,20); w9=w17^ror(w9,20); w10=w1^ror(w10,20);
    a=ror(w11,25); b=ror(w8,25); c=ror(w9,25); d=ror(w10,25);
    w6+=b; w4+=c; w5+=d; w3+=a;
    w0=w6^ror(w0,24); w15=w4^ror(w15,24); w16=w5^ror(w16,24); w2=w3^ror(w2,24);
    a=ror(w0,16); b=ror(w15,16); c=ror(w16,16); d=ror(w2,16);
    w17+=a; w1+=b; w13+=c; w14+=d;
    w8=w17^ror(w8,25); w9=w1^ror(w9,25); w10=w13^ror(w10,25); w11=w14^ror(w11,25);
    a=ror(w8,20); b=ror(w9,20); c=ror(w10,20); d=ror(w11,20);
    w6+=a; w4+=b; w5+=c; w3+=d;
    w0=w6^ror(w0,16); w15=w4^ror(w15,16); w16=w5^ror(w16,16); w2=w3^ror(w2,16);
    w0=ror(w0,24); w15=ror(w15,24); w16=ror(w16,24); w2=ror(w2,24);
    w17+=w0; w1+=w15; w13+=w16; w14+=w2;
    w8=ror(w17^ror(w8,20),25); w9=ror(w1^ror(w9,20),25);
    w10=ror(w13^ror(w10,20),25); w11=ror(w14^ror(w11,20),25);
  }
  return x;
}

}  // namespace

DeviceProfile profile_from_raw_json(const std::string& raw_json) {
  json raw = json::parse(raw_json);
  DeviceProfile p;
  auto value = [](const json& object, const char* key) {
    if (!object.is_object() || !object.contains(key)) return std::string();
    const auto& v = object.at(key);
    return v.is_string() ? v.get<std::string>() : v.dump();
  };
  const json& props = raw.value("props", json::object());
  const json& device = raw.value("device", json::object());
  const json& app = raw.value("app", json::object());
  const json& files = raw.value("files", json::object());
  const json& qimei = raw.value("qimei", json::object());
  if (!qimei.is_object() || qimei.empty())
    throw std::runtime_error(
        "Raw fingerprint is missing the qimei group; update the Tencent Helper APK");
  p.app_version=value(app,"qqlive_version_name");
  if (p.app_version.empty()) p.app_version="9.02.90.31109";
  p.android_version=value(props,"ro.build.version.release");
  p.android_api_level=value(props,"ro.build.version.sdk");
  p.model=value(props,"ro.product.model");
  p.manufacturer=value(props,"ro.product.manufacturer");
  p.brand=value(props,"ro.product.brand");
  p.board=value(props,"ro.product.board");
  p.device=value(props,"ro.product.device");
  p.product_name=value(props,"ro.product.name");
  p.build_host=value(props,"ro.build.host");
  p.first_api_level=value(props,"ro.product.first_api_level");
  if (p.first_api_level.empty())
    p.first_api_level=value(props,"ro.board.first_api_level");
  if (p.first_api_level.empty())
    p.first_api_level=value(props,"ro.board.api_level");
  p.kernel=value(device,"kernel");
  if (p.kernel.empty()) p.kernel=value(qimei,"kernel");  // Legacy snapshot.
  p.boot_id=value(files,"boot_id");
  try {
    std::string timezone=value(device,"timezone_offset_minutes");
    if (timezone.empty()) timezone=value(qimei,"timezone_offset_minutes");
    p.timezone_offset_minutes=std::stoi(timezone);
  } catch (...) {}
  try { p.current_time_ms=std::stoll(value(device,"current_time_ms")); } catch (...) {}
  try { p.elapsed_realtime_ms=std::stoll(value(device,"elapsed_realtime_ms")); } catch (...) {}
  const json& packages = raw.value("packages", json::array());
  for (const auto& pkg : packages) {
    if (value(pkg,"name")==kPackage) {
      try { p.first_install_ms=std::stoll(value(pkg,"first_install_ms")); } catch (...) {}
      p.android_id=value(pkg,"android_id");
      break;
    }
  }
  if (p.android_id.empty())
    throw std::runtime_error(
        "com.tencent.qqlive android_id is missing; the helper needs root access");
  bool has_direct_signal =
      qimei.contains("device_signal_kv") &&
      qimei["device_signal_kv"].is_string() &&
      !qimei["device_signal_kv"].get<std::string>().empty();
  bool has_stat_records =
      qimei.contains("stat_records") && qimei["stat_records"].is_array() &&
      !qimei["stat_records"].empty();
  if (!has_direct_signal && !has_stat_records)
    throw std::runtime_error(
        "Raw fingerprint is missing QIMEI stat records; update the Tencent Helper APK");
  p.device_signal_kv =
      build_device_signal(qimei, p.boot_id, p.timezone_offset_minutes);
  return p;
}

std::string build_tail17(const DeviceProfile& p) {
  std::string source;
  source += p.device_signal_kv;
  source += "";  // qimei16
  source += "";  // qimei36
  source += p.android_id;
  source += "";  // slot6
  source += "Android " + p.android_version + ",level " + p.android_api_level;
  source += p.model;
  source += kAppKey;
  source += kSdkVersion;
  source += p.app_version;
  Bytes data = bytes(source);
  static const Bytes salt = bytes("plzdontcrackme856c91q16");
  std::size_t total=data.size()+20;
  Bytes odd(total/2), even(total-total/2);
  for (std::size_t i=0;i<total;++i) {
    if (i&1) odd[i/2]=i<data.size()?data[i]:salt[i-data.size()];
    else even[i/2]=i<=19?salt[i]:data[i-20];
  }
  auto eh=h14(even), oh=h14(odd);
  Bytes out; out.reserve(32);
  for (uint8_t b : eh) out.push_back(oh[b&0x1f]);
  return util::hex_encode(out);
}

std::string build_register_plaintext(const DeviceProfile& p,
                                     const std::string& req_id,
                                     int64_t now_sec) {
  std::string qreq;
  pb_int(qreq,1,1); pb_string(qreq,2,req_id); pb_int(qreq,3,0);
  pb_string(qreq,4,kAppKey); pb_string(qreq,5,kSdkVersion);
  pb_string(qreq,6,p.app_version);
  pb_string(qreq,7,"Android "+p.android_version+",level "+p.android_api_level);
  pb_string(qreq,8,p.model); pb_string(qreq,9,""); pb_string(qreq,10,"");
  pb_string(qreq,11,kPackage); pb_string(qreq,12,"wifi"); pb_int(qreq,13,7);

  std::string context;
  pb_string(context,1,kBuildId); pb_string(context,2,"33"); pb_string(context,3,"{}");
  pb_string(context,4,p.manufacturer); pb_string(context,5,p.device_signal_kv);
  pb_string(context,6,""); pb_string(context,7,""); pb_string(context,8,"");
  pb_string(context,9,p.android_id); pb_string(context,10,""); pb_string(context,11,"");
  pb_string(context,12,rich_json(p)); pb_string(context,13,"");
  pb_string(context,14,"Phone");
  std::string boot_epoch;
  if (p.current_time_ms && p.elapsed_realtime_ms)
    boot_epoch=std::to_string((p.current_time_ms-p.elapsed_realtime_ms)/1000);
  pb_string(context,15,boot_epoch);
  pb_string(context,16,p.boot_id.empty()?"":rc4_hex(p.boot_id));
  pb_string(context,17,build_tail17(p)); pb_string(context,18,field18(now_sec));
  std::string out; pb_msg(out,1,qreq); pb_msg(out,2,context);
  return out;
}

std::string make_sn(const std::string& preimage, uint32_t state_word) {
  Bytes md5=digest(EVP_md5(),preimage);
  std::array<uint32_t,16> state={
      state_word,0,0,0,0x61707865,0x3320646E,0x79622D32,0x6B206574,
      0,0,0xEF44A2A3,0x65A4E57F,0,0,0,0};
  auto rounded=sn_rounds(state);
  std::array<uint8_t,16> stream{};
  store_le32(stream.data(),rounded[4]+0x61707865);
  store_le32(stream.data()+4,rounded[5]+0x3320646E);
  store_le32(stream.data()+8,rounded[6]+0x79622D32);
  store_le32(stream.data()+12,rounded[7]+0x6B206574);
  Bytes phase1(16);
  for (int i=0;i<16;++i) phase1[i]=stream[i]^md5[i];
  Bytes key=util::hex_to_bytes("6db579083ffb211c7c8d917494fc4561");
  key.insert(key.end(),{0x28,phase1[5],0x47,0x2b,md5[4],0x2c,0x4c,0x4b});
  Bytes mask=digest(EVP_md5(),string_bytes(key));
  for (int i=0;i<16;++i) mask[i]^=key[16+(i%8)];
  Bytes seed(md5.begin(),md5.begin()+4); seed.insert(seed.end(),4,0);
  seed.insert(seed.end(),phase1.begin()+1,phase1.begin()+5);
  seed.insert(seed.end(),{0x78,0x56,0x34,0x12});
  for (int i=0;i<16;++i) seed[i]^=mask[i];
  Bytes out={0x4c,0x43,0x85,0x10,md5[4],phase1[5],
             static_cast<uint8_t>(state_word>>24),static_cast<uint8_t>(state_word>>16),
             static_cast<uint8_t>(state_word>>8),static_cast<uint8_t>(state_word)};
  out.insert(out.end(),seed.begin(),seed.end()); out.insert(out.end(),8,0);
  return util::hex_encode(out);
}

Result register_device(const DeviceProfile& profile, long timeout_sec) {
  int64_t now_ms=std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  std::string inner=build_register_plaintext(profile,request_id(),now_ms/1000);
  Bytes inner_cipher=aes_cbc(bytes(inner),util::hex_to_bytes(kInnerKeyHex),util::hex_to_bytes(kInnerIvHex),true);
  Bytes wrapped={0x03,0x00,0x00,0x00,0x01};
  wrapped.insert(wrapped.end(),inner_cipher.begin(),inner_cipher.end());
  Bytes key=random_bytes(16),iv=random_bytes(16);
  Bytes outer=aes_cbc(wrapped,key,iv,true);
  Bytes key_iv=key; key_iv.insert(key_iv.end(),iv.begin(),iv.end());
  std::string ky=standard_b64(rsa_wrap(key_iv));
  std::string pms=standard_b64(outer);
  std::string nn=util::hex_encode(random_bytes(8));
  uint32_t state=0; Bytes state_bytes=random_bytes(3);
  state=(uint32_t(state_bytes[0])<<16)|(uint32_t(state_bytes[1])<<8)|state_bytes[2];
  state&=0xfffff;
  std::string tm=std::to_string(now_ms);
  std::string sn=make_sn("1"+ky+pms+tm+nn,state);
  json body;
  body["cpt"]="1"; body["ext"]=json({{"app_key",kAppKey}}).dump();
  body["ky"]=ky; body["nn"]=nn; body["pms"]=pms; body["sn"]=sn; body["tm"]=tm;
  OrderedKV headers={{"Content-Type","application/json"},
                     {"User-Agent","Dalvik/2.1.0 (Linux; U; Android "+profile.android_version+
                                   "; "+profile.model+" Build/unknown)"},
                     {"Accept-Encoding","identity"}};
  auto response=http::http_post(kUrl,headers,body.dump(),timeout_sec);
  if (response.first!=200)
    throw std::runtime_error("Qimei REGISTER HTTP "+std::to_string(response.first));
  json doc=json::parse(response.second);
  if (!doc.contains("data") || !doc["data"].is_string() || doc["data"].get<std::string>().empty())
    throw std::runtime_error("Qimei REGISTER response has no encrypted data");
  Bytes plain=aes_cbc(standard_b64_decode(doc["data"].get<std::string>()),key,iv,false);
  Result result{protobuf_string(plain,1),protobuf_string(plain,2)};
  if (result.qimei36.empty())
    throw std::runtime_error("Qimei REGISTER backend returned no qimei36");
  return result;
}

}  // namespace tencent::qimei
