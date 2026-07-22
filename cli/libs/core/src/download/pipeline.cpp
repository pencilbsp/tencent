#include "tencent/core/download/pipeline.h"

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/provider.h>
#endif

extern "C" {
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/log.h>
}

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "tencent/core/protocol/playback/ckey_codec.h"
#include "tencent/core/download/proxy_config.h"
#include "tencent/core/protocol/playback/constants.h"
#include "tencent/core/common/json.h"
#include "tencent/core/common/bytes.h"
#include "tencent/core/common/logging.h"
#include "tencent/core/storage/paths.h"
#include "tencent/core/common/hex.h"
#include "tencent/core/crypto/qqtea.h"
#include "tencent/core/crypto/tencent_base64.h"
#include "tencent/core/crypto/xor_cipher.h"

namespace fs = std::filesystem;

namespace tencent::download {
namespace {

// Bytes lấy từ common/bytes.h (namespace tencent) — không cần khai báo lại.

constexpr uint8_t kTsSync = 0x47;
constexpr size_t kTsPacketSize = 188;
constexpr int kInitialClear = 48;
constexpr int kProtectedLen = 64;
constexpr int kStride = 192;

Bytes http_get_bytes(const std::string& url,
                     const std::vector<std::pair<std::string, std::string>>& headers,
                     long timeout,
                     std::atomic<uint64_t>* progress_bytes);

const char kAndroidPrivateKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIICdgIBADANBgkqhkiG9w0BAQEFAASCAmAwggJcAgEAAoGBAMFljgC9KAfMZoXR\n"
    "U/3Vkt4yJ+N468FFN16MEERCymiS0mH67VwWbcWBD0acARvxaBu8OUoxszAg4/sj\n"
    "E0o4S7AQ18XMT4VnIUt96EDLubr1Q00AS2cE48BinIv+dg/K49ppOMs8kbBG33yk\n"
    "wZMOogqsM/RdNgaZ8uc9xnPMjed3AgMBAAECgYEAm0wDVZCPXoPCJ97JxWEFCKvZ\n"
    "4A2vsoj0R1XNkiLYClUr30Xx6CqShzB1pcuMEHYvlWX10yBJtXIT4AxwZv8BtsC/\n"
    "pmGlG+7nEFGmyGmYkdHuMeRaWNU5Iu6DxQHaSls1bzBytmDcV8NDvDPkV7czDamb\n"
    "RvdI4kdnkpxM4P6Jj7ECQQDjdSybxsEWGMHEN+jf3LZKZfLBWaw0G9bVeRw17f3B\n"
    "TGUe/KPHGzA4Naxu5P6V8VCianCwhPIDPUjUXMF0dh1zAkEA2ao1UxMlGdNzUN9k\n"
    "ib3FfBPidZnNf1RhGnV74NuGayfG1F5FE3pyejrdtNUjoGuCm9W2NtNCuO1bOUQx\n"
    "1g/M7QJAR5tj58ayktpIZhWT4qYBXXqq6ePpKp55C8uM7g12jZwXWaH/jXcpvwhm\n"
    "uKKu41WNJRSNVYUFC7AHdZOd86QbsQJATsHAc6Osu/B/fBaoiRJKP3/uY8DX0yeT\n"
    "zwANZ6nmtCT7U2IculSG0WU1TTYFowjjhEFKogHE1abEaC8dnSewbQJAYA08tMCg\n"
    "yJXc6cY90J1SDy50Gy5W6b0gOyTw7m1FdUsalSUFgMVHRLTFhjue6hSJqETkTcyY\n"
    "jTpl1LlGC4W83g==\n"
    "-----END PRIVATE KEY-----\n";

const char kIosPrivateKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIICdwIBADANBgkqhkiG9w0BAQEFAASCAmEwggJdAgEAAoGBAMXOIaYw7QqzMnfQ\n"
    "ClsjlFsXSUHyUgpR8fj+eluk9xBREx6d7ONNSgBnRIbX5UUkCAFW5syffe8I3Fv0\n"
    "9L7corCS8Tmy32qIfWfNZq/Y1yfBRlUiUFx4p0fTRV6nTjnWpFWOYqO21tFuyo6y\n"
    "E5EW0/NZhAvthH56zVeP+m0f6+XBAgMBAAECgYBL9m1VtVRS1556BJr1hSRkFv07\n"
    "JiP7cpzKUnAK4b8/tOLyzxM592SD4ySd5CtX8g5g9U6NJnLmaTC238tdwL15BRVO\n"
    "9Lg+jsYA7EXZdR8iMCCCzhXxuI8UTN5NJJiB2IQ3fHCi4mkbyBtUdJjQXiRvASml\n"
    "NJRK2DRxUKwAnO4fAQJBAOXCVPRAikM06i/B41bjBx4cBiZVDdqnJFc4F+go/VGL\n"
    "hfLzCjLWVxHu1Cj+kA6COg8qOPBegr6g84dI6juMCSUCQQDcZYkEM+sabzo3NRi5\n"
    "XNyd7hAEB62DdvmzMkfbPjvbx3ARgdWJvR6Pub/oJ1gH8eUszkkpJ/ExMf1zpSEn\n"
    "Rq1tAkEA2eYlv8f92KM0frp5KwwA0hNt2ajMjNP7aSRvt+Q36sBR9wl8YJ5a0U0Z\n"
    "tQ9H/BqRiZRXj89yW+w1SdZjHro+aQJBALrveBbTfo8OVn9rDsxyQkHGIVl5tadz\n"
    "K3ndpOsFzzmr4JdI23kXqqtXbrmXXWhnuO2XDqDU2xPSBdqn7gXlQcUCQFNldtAC\n"
    "wYsbd27iU60e8rvPjnPT9rxbrMUtAjP34SJyf/LOl6MCATrYQChD3WxbmFkiuFmt\n"
    "Tnkju7sH+xO6h1I=\n"
    "-----END PRIVATE KEY-----\n";

constexpr const char* kIosCkeyTeaKeyHex = "4975e3bcefb129de1f6091a5b5869a3d";

[[noreturn]] void fail(const std::string& msg) { throw std::runtime_error(msg); }

void trace(bool enabled, const std::string& msg) {
  if (enabled) log_err() << msg << "\n";
}

std::string read_text(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) fail("Unable to open file: " + path.string());
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

Bytes read_bytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) fail("Unable to open file: " + path.string());
  return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_text(const fs::path& path, const std::string& text) {
  if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) fail("Unable to write file: " + path.string());
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void write_bytes(const fs::path& path, const Bytes& data) {
  if (!path.parent_path().empty()) fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) fail("Unable to write file: " + path.string());
  out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

uint64_t file_size_or_zero(const fs::path& path) {
  std::error_code ec;
  auto sz = fs::file_size(path, ec);
  return ec ? 0 : static_cast<uint64_t>(sz);
}

fs::path abs_path(const std::string& path) {
  std::error_code ec;
  fs::path p = fs::absolute(fs::path(path), ec);
  return ec ? fs::path(path) : p;
}

std::string av_error(int code) {
  std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer{};
  av_strerror(code, buffer.data(), buffer.size());
  return buffer.data();
}

void check_av(int code, const std::string& operation) {
  if (code < 0) fail(operation + ": " + av_error(code));
}

Bytes bytes_from_ascii(const std::string& s) {
  return Bytes(s.begin(), s.end());
}

std::string ascii_from_bytes(const Bytes& b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

std::string trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

std::string lower_ascii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool is_ios_anc_profile(const std::string& profile) {
  std::string normalized = lower_ascii(trim(profile));
  return normalized == "ios" || normalized == "iphone" || normalized == "ipad" ||
         normalized == "apple";
}

bool is_auto_anc_profile(const std::string& profile) {
  std::string normalized = lower_ascii(trim(profile));
  return normalized.empty() || normalized == "auto";
}

struct AncMaterial {
  std::string profile;
  std::string rsa_private_pem;
  Bytes ckey_tea_key;
  Bytes ckey_xor_key;
};

AncMaterial resolve_anc_material_for_profile(const AncOptions& options,
                                             const std::string& requested_profile) {
  std::string profile = lower_ascii(trim(requested_profile));
  const bool ios = is_ios_anc_profile(profile);
  if (!ios && profile != "android")
    fail("Invalid ANC profile: " + requested_profile + " (expected auto|android|ios|apple)");

  AncMaterial material;
  material.profile = ios ? "ios" : "android";
  if (!options.rsa_private_key_path.empty())
    material.rsa_private_pem = read_text(abs_path(options.rsa_private_key_path));
  else
    material.rsa_private_pem = ios ? kIosPrivateKeyPem : kAndroidPrivateKeyPem;

  const std::string tea_hex = !options.ckey_tea_key_hex.empty()
                                  ? options.ckey_tea_key_hex
                                  : (ios ? kIosCkeyTeaKeyHex
                                         : playback::constants::kCkeyTeaKeyHex);
  const std::string xor_hex = !options.ckey_xor_key_hex.empty()
                                  ? options.ckey_xor_key_hex
                                  : playback::constants::kCkeyXorKeyHex;
  material.ckey_tea_key = util::hex_to_bytes(tea_hex);
  material.ckey_xor_key = util::hex_to_bytes(xor_hex);
  if (material.ckey_tea_key.size() != 16)
    fail("cKey TEA key must be 16 bytes of hex");
  if (material.ckey_xor_key.empty())
    fail("cKey XOR key must not be empty");
  return material;
}

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
      };
      int hi = hex(s[i + 1]);
      int lo = hex(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    if (s[i] == '+') out.push_back(' ');
    else out.push_back(s[i]);
  }
  return out;
}

std::string request_form_value(const std::string& request_text,
                               const std::string& key) {
  // Serialized requests and app curl captures both retain the URL-encoded form
  // body. Restrict keys to internal constants before constructing this regex.
  std::regex rx("(?:^|[&\\s'\\\"])" + key + "=([^&\\s'\\\"]*)");
  std::smatch match;
  if (!std::regex_search(request_text, match, rx)) return "";
  return url_decode(match[1].str());
}

std::optional<std::pair<std::string, std::string>> detect_anc_profile_from_request(
    const std::string& request_text) {
  const std::string platform =
      lower_ascii(trim(request_form_value(request_text, "platform")));
  auto is_known_ios_platform = [](const std::string& value) {
    return value == "10403" || value == "10103";
  };
  auto is_known_android_platform = [](const std::string& value) {
    return value == "10303" || value == "10503";
  };

  if (is_known_ios_platform(platform))
    return std::make_pair(std::string("ios"), "platform=" + platform);
  if (is_known_android_platform(platform))
    return std::make_pair(std::string("android"), "platform=" + platform);
  if (platform.empty())
    fail("Could not detect ANC profile: request has no platform");
  fail("Platform does not support automatic ANC profile detection: " + platform +
       " (android=10303/10503, ios=10403/10103)");
}

std::vector<AncMaterial> resolve_anc_materials(const AncOptions& options,
                                               const std::string& request_text,
                                               bool trace_enabled) {
  std::string profile = lower_ascii(trim(options.profile));
  if (!is_auto_anc_profile(profile))
    return {resolve_anc_material_for_profile(options, profile)};

  if (auto detected = detect_anc_profile_from_request(request_text)) {
    trace(trace_enabled, "[anc] auto profile=" + detected->first +
                             " from request " + detected->second);
    return {resolve_anc_material_for_profile(options, detected->first)};
  }

  fail("Could not detect ANC profile from request");
}

std::string cache_component(std::string value, bool lowercase) {
  if (lowercase) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
  }
  for (char& c : value) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!std::isalnum(uc) && c != '.' && c != '_' && c != '-') c = '_';
  }
  if (value.empty() || value == "." || value == "..")
    fail("Invalid VID/quality cache component");
  return value;
}

void init_openssl_providers() {
#if OPENSSL_VERSION_MAJOR >= 3
  static std::once_flag once;
  std::call_once(once, [] {
    OSSL_PROVIDER_load(nullptr, "default");
    OSSL_PROVIDER_load(nullptr, "legacy");
  });
#endif
}

Bytes evp_decrypt(const Bytes& ciphertext, const EVP_CIPHER* cipher, const Bytes& key,
                  const Bytes& iv, bool padding = true) {
  init_openssl_providers();
  EVP_CIPHER_CTX* raw = EVP_CIPHER_CTX_new();
  if (!raw) fail("EVP_CIPHER_CTX_new failed");
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(raw, EVP_CIPHER_CTX_free);
  if (EVP_DecryptInit_ex(ctx.get(), cipher, nullptr, key.data(), iv.data()) != 1)
    fail("EVP_DecryptInit_ex failed");
  EVP_CIPHER_CTX_set_padding(ctx.get(), padding ? 1 : 0);
  Bytes out(ciphertext.size() + EVP_CIPHER_block_size(cipher));
  int out_len1 = 0;
  if (EVP_DecryptUpdate(ctx.get(), out.data(), &out_len1, ciphertext.data(),
                        static_cast<int>(ciphertext.size())) != 1)
    fail("EVP_DecryptUpdate failed");
  int out_len2 = 0;
  if (EVP_DecryptFinal_ex(ctx.get(), out.data() + out_len1, &out_len2) != 1)
    fail("EVP_DecryptFinal_ex failed");
  out.resize(static_cast<size_t>(out_len1 + out_len2));
  return out;
}

Bytes aes_cbc_decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
  if (iv.size() != 16) fail("AES-CBC IV must be 16 bytes");
  const EVP_CIPHER* cipher = nullptr;
  if (key.size() == 16) cipher = EVP_aes_128_cbc();
  else if (key.size() == 24) cipher = EVP_aes_192_cbc();
  else if (key.size() == 32) cipher = EVP_aes_256_cbc();
  else fail("Invalid AES-CBC key length: " + std::to_string(key.size()));
  return evp_decrypt(ciphertext, cipher, key, iv);
}

Bytes des_cbc_decrypt(const Bytes& ciphertext, const Bytes& key, const Bytes& iv) {
  if (key.size() != 8 || iv.size() != 8) fail("DES-CBC requires an 8-byte key and IV");
  return evp_decrypt(ciphertext, EVP_des_cbc(), key, iv);
}

Bytes rsa_oaep_sha1_decrypt(const Bytes& ciphertext,
                            const std::string& private_key_pem) {
  BIO* bio = BIO_new_mem_buf(private_key_pem.data(),
                             static_cast<int>(private_key_pem.size()));
  if (!bio) fail("BIO_new_mem_buf failed");
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_ptr(bio, BIO_free);
  EVP_PKEY* raw_key = PEM_read_bio_PrivateKey(bio_ptr.get(), nullptr, nullptr, nullptr);
  if (!raw_key) fail("PEM_read_bio_PrivateKey failed");
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(raw_key, EVP_PKEY_free);
  EVP_PKEY_CTX* raw_ctx = EVP_PKEY_CTX_new(pkey.get(), nullptr);
  if (!raw_ctx) fail("EVP_PKEY_CTX_new failed");
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(raw_ctx, EVP_PKEY_CTX_free);
  if (EVP_PKEY_decrypt_init(ctx.get()) <= 0) fail("EVP_PKEY_decrypt_init failed");
  if (EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_OAEP_PADDING) <= 0)
    fail("EVP_PKEY_CTX_set_rsa_padding failed");
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx.get(), EVP_sha1()) <= 0)
    fail("EVP_PKEY_CTX_set_rsa_oaep_md failed");
  if (EVP_PKEY_CTX_set_rsa_mgf1_md(ctx.get(), EVP_sha1()) <= 0)
    fail("EVP_PKEY_CTX_set_rsa_mgf1_md failed");
  size_t out_len = 0;
  if (EVP_PKEY_decrypt(ctx.get(), nullptr, &out_len, ciphertext.data(), ciphertext.size()) <= 0)
    fail("RSA decrypt sizing failed");
  Bytes out(out_len);
  if (EVP_PKEY_decrypt(ctx.get(), out.data(), &out_len, ciphertext.data(), ciphertext.size()) <= 0)
    fail("RSA-OAEP-SHA1 decrypt failed");
  out.resize(out_len);
  return out;
}

uint32_t rotl32(uint32_t value, int shift) {
  return (value << shift) | (value >> (32 - shift));
}

uint32_t ror32(uint32_t value, int shift) {
  return (value >> shift) | (value << (32 - shift));
}

uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

void put_le32(Bytes& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 24));
}

void chacha_qr(std::array<uint32_t, 16>& s, int a, int b, int c, int d) {
  s[a] += s[b]; s[d] ^= s[a]; s[d] = rotl32(s[d], 16);
  s[c] += s[d]; s[b] ^= s[c]; s[b] = rotl32(s[b], 12);
  s[a] += s[b]; s[d] ^= s[a]; s[d] = rotl32(s[d], 8);
  s[c] += s[d]; s[b] ^= s[c]; s[b] = rotl32(s[b], 7);
}

Bytes chacha20_block_rfc(const Bytes& key32, const Bytes& nonce12, uint32_t counter) {
  if (key32.size() != 32 || nonce12.size() != 12) fail("Invalid ChaCha20 key/nonce length");
  std::array<uint32_t, 16> init{
      0x61707865u, 0x3320646eu, 0x79622d32u, 0x6b206574u,
      le32(&key32[0]), le32(&key32[4]), le32(&key32[8]), le32(&key32[12]),
      le32(&key32[16]), le32(&key32[20]), le32(&key32[24]), le32(&key32[28]),
      counter, le32(&nonce12[0]), le32(&nonce12[4]), le32(&nonce12[8])};
  auto s = init;
  for (int i = 0; i < 10; ++i) {
    chacha_qr(s, 0, 4, 8, 12); chacha_qr(s, 1, 5, 9, 13);
    chacha_qr(s, 2, 6, 10, 14); chacha_qr(s, 3, 7, 11, 15);
    chacha_qr(s, 0, 5, 10, 15); chacha_qr(s, 1, 6, 11, 12);
    chacha_qr(s, 2, 7, 8, 13); chacha_qr(s, 3, 4, 9, 14);
  }
  Bytes out;
  out.reserve(64);
  for (int i = 0; i < 16; ++i) put_le32(out, s[i] + init[i]);
  return out;
}

Bytes chacha20_crypt_rfc(const Bytes& data, Bytes key, const Bytes& nonce, uint32_t counter = 0) {
  if (key.size() > 32) key.resize(32);
  key.resize(32, 0);
  if (nonce.size() != 12) fail("ChaCha20 nonce must be 12 bytes");
  Bytes out(data.size());
  size_t pos = 0;
  uint32_t block_counter = counter;
  while (pos < data.size()) {
    Bytes block = chacha20_block_rfc(key, nonce, block_counter++);
    size_t take = std::min<size_t>(64, data.size() - pos);
    for (size_t i = 0; i < take; ++i) out[pos + i] = data[pos + i] ^ block[i];
    pos += take;
  }
  return out;
}

Bytes sha256_bytes(const std::string& s) {
  Bytes out(SHA256_DIGEST_LENGTH);
  SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), out.data());
  return out;
}

Bytes rc4_crypt(const Bytes& data, const Bytes& key) {
  if (key.empty()) fail("RC4 key must not be empty");
  std::array<uint8_t, 256> s{};
  for (int i = 0; i < 256; ++i) s[i] = static_cast<uint8_t>(i);
  int j = 0;
  for (int i = 0; i < 256; ++i) {
    j = (j + s[i] + key[i % key.size()]) & 0xff;
    std::swap(s[i], s[j]);
  }
  Bytes out;
  out.reserve(data.size());
  int i = 0;
  j = 0;
  for (uint8_t byte : data) {
    i = (i + 1) & 0xff;
    j = (j + s[i]) & 0xff;
    std::swap(s[i], s[j]);
    out.push_back(byte ^ s[(s[i] + s[j]) & 0xff]);
  }
  return out;
}

Bytes repeating_xor(const Bytes& data, const Bytes& key) {
  if (key.empty()) fail("XOR key must not be empty");
  Bytes out(data.size());
  for (size_t i = 0; i < data.size(); ++i) out[i] = data[i] ^ key[i % key.size()];
  return out;
}

uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) |
         uint32_t(p[3]);
}

void put_be32(Bytes& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void write_be32(Bytes& out, size_t pos, uint32_t v) {
  if (pos + 4 > out.size()) fail("write_be32 would exceed the buffer");
  out[pos] = static_cast<uint8_t>(v >> 24);
  out[pos + 1] = static_cast<uint8_t>(v >> 16);
  out[pos + 2] = static_cast<uint8_t>(v >> 8);
  out[pos + 3] = static_cast<uint8_t>(v);
}

Bytes tea_decrypt_block_anc(const Bytes& block, const Bytes& key) {
  if (block.size() != 8 || key.size() != 16) fail("TEA block decrypt requires block=8 and key=16");
  uint32_t y = be32(&block[0]), z = be32(&block[4]);
  uint32_t k0 = be32(&key[0]), k1 = be32(&key[4]), k2 = be32(&key[8]), k3 = be32(&key[12]);
  uint32_t delta = 0x9e3779b9u;
  uint32_t total = delta * 16u;
  for (int i = 0; i < 16; ++i) {
    z -= (((y << 4) + k2) ^ (y + total) ^ ((y >> 5) + k3));
    y -= (((z << 4) + k0) ^ (z + total) ^ ((z >> 5) + k1));
    total -= delta;
  }
  Bytes out;
  put_be32(out, y);
  put_be32(out, z);
  return out;
}

Bytes xor_equal(const Bytes& a, const Bytes& b) {
  if (a.size() != b.size()) fail("xor_equal length mismatch");
  Bytes out(a.size());
  for (size_t i = 0; i < a.size(); ++i) out[i] = a[i] ^ b[i];
  return out;
}

Bytes qq_tea_decrypt_anc(const Bytes& data, const Bytes& key) {
  if (key.size() != 16) fail("QQ-TEA key must be 16 bytes");
  if (data.size() < 16 || data.size() % 8 != 0) fail("Invalid QQ-TEA ciphertext length");
  Bytes prev_plain(8, 0), prev_cipher(8, 0), plain;
  plain.reserve(data.size());
  for (size_t off = 0; off < data.size(); off += 8) {
    Bytes block(data.begin() + off, data.begin() + off + 8);
    Bytes dec = tea_decrypt_block_anc(xor_equal(block, prev_plain), key);
    Bytes cur = xor_equal(dec, prev_cipher);
    plain.insert(plain.end(), cur.begin(), cur.end());
    prev_plain = dec;
    prev_cipher = block;
  }
  size_t pad_len = plain[0] & 0x07;
  size_t start = pad_len + 3;
  size_t end = plain.size() - 7;
  if (end < start) fail("QQ-TEA framing invalid");
  for (size_t i = end; i < plain.size(); ++i) {
    if (plain[i] != 0) fail("QQ-TEA trailing zero invalid");
  }
  return Bytes(plain.begin() + static_cast<std::ptrdiff_t>(start),
               plain.begin() + static_cast<std::ptrdiff_t>(end));
}

std::string extract_ckey_from_request(const std::string& request_text) {
  std::regex rx("cKey=([^&\\s'\\\"]+)");
  std::smatch m;
  if (!std::regex_search(request_text, m, rx)) fail("Could not find cKey in request");
  return url_decode(m[1].str());
}

Bytes decode_ckey_payload_raw(const std::string& ckey,
                              const AncMaterial& material) {
  if (ckey.rfind("--01", 0) != 0)
    fail("cKey must start with --01");
  Bytes ct = crypto::tencent_b64_decode(ckey.substr(4));
  Bytes pre = crypto::xor_bytes(ct, material.ckey_xor_key);
  if (pre.size() < 12)
    fail("cKey payload is too short");
  Bytes cipher(pre.begin(), pre.end() - 4);
  Bytes plain = crypto::qqtea_cbc_decrypt(cipher, material.ckey_tea_key);
  if (plain.size() < 10)
    fail("cKey plaintext is too short");
  const size_t pad_count = plain[0] & 7;
  const size_t start = 3 + pad_count;
  const size_t end = plain.size() - 7;
  if (start > end || end > plain.size())
    fail("cKey QQ-TEA framing invalid");
  return Bytes(plain.begin() + static_cast<std::ptrdiff_t>(start),
               plain.begin() + static_cast<std::ptrdiff_t>(end));
}

void append_scan_text(std::string& all, const Bytes& data) {
  all.append(reinterpret_cast<const char*>(data.data()), data.size());
  all.push_back('\n');
}

std::vector<std::pair<std::string, std::string>> request_headers_for_segments(
    const std::string& request_text, const std::string& download_user_agent) {
  std::vector<std::pair<std::string, std::string>> headers;
  auto add_header = [&](const std::string& raw) {
    auto pos = raw.find(':');
    if (pos == std::string::npos) return;
    std::string key = trim(raw.substr(0, pos));
    std::string value = trim(raw.substr(pos + 1));
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "host" || lower == "content-type" || lower == "content-length" ||
        lower == "expect" || lower == "accept-encoding")
      return;
    for (auto& header : headers) {
      std::string existing = header.first;
      std::transform(existing.begin(), existing.end(), existing.begin(),
                     [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (existing == lower) {
        header.second = value;
        return;
      }
    }
    headers.emplace_back(key, value);
  };

  bool parsed_curl_headers = false;
  std::regex rx("-H '([^']+)'");
  for (auto it = std::sregex_iterator(request_text.begin(), request_text.end(), rx);
       it != std::sregex_iterator(); ++it) {
    parsed_curl_headers = true;
    add_header((*it)[1].str());
  }

  if (!parsed_curl_headers) {
    std::istringstream lines(request_text);
    std::string line;
    bool first = true;
    while (std::getline(lines, line)) {
      if (first) {
        first = false;
        continue;
      }
      if (trim(line).empty()) break;
      add_header(line);
    }
  }

  bool has_user_agent = false;
  for (auto& header : headers) {
    std::string key = header.first;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key == "user-agent") {
      has_user_agent = true;
      if (!download_user_agent.empty() && trim(header.second) == "qqlive")
        header.second = download_user_agent;
    }
  }
  if (!has_user_agent && !download_user_agent.empty())
    headers.emplace_back("User-Agent", download_user_agent);
  return headers;
}

std::pair<Bytes, Bytes> derive_rc_prestage_from_ckey(const std::string& request_text,
                                                     const AncMaterial& material,
                                                     bool trace_enabled) {
  std::string ckey = extract_ckey_from_request(request_text);
  std::string all;
  append_scan_text(all, decode_ckey_payload_raw(ckey, material));
  try {
    playback::CkeyDecoded decoded =
        playback::decode_ckey(ckey, material.ckey_xor_key, material.ckey_tea_key);
    for (const auto& field : decoded.fields)
      append_scan_text(all, field);
    if (decoded.fields.size() >= 20)
      append_scan_text(all, decoded.fields[19]);
  } catch (const std::exception& e) {
    trace(trace_enabled, "[ckey] structured parse skipped: " + std::string(e.what()));
  }
  std::smatch dd, sd;
  std::regex dd_rx("\"dd\"\\s*:\\s*\"([0-9a-fA-F]{16,64})\"");
  std::regex sd_rx("\"(sd|param2)\"\\s*:\\s*\"(\\d{13}[0-9a-fA-F]{16})\"");
  Bytes key, iv;
  if (std::regex_search(all, dd, dd_rx)) {
    key = bytes_from_ascii(dd[1].str().substr(0, 16));
    trace(trace_enabled, "[rc] rc-pre-key from cKey dd=" + dd[1].str().substr(0, 16));
  }
  if (std::regex_search(all, sd, sd_rx)) {
    iv = bytes_from_ascii(sd[2].str().substr(0, 16));
    trace(trace_enabled, "[rc] rc-pre-iv from cKey " + sd[1].str() + "=" +
                             sd[2].str().substr(0, 16));
  }
  if (key.empty()) fail("Could not derive rc-pre-key/dd from cKey");
  if (iv.empty()) fail("Could not derive rc-pre-iv/sd from cKey");
  return {key, iv};
}

std::string load_outer_anc_text(const std::string& response_text) {
  std::string text = trim(response_text);
  if (text.empty()) fail("Response is empty");
  if (text[0] != '{') return text;
  nlohmann::json parsed = nlohmann::json::parse(text);
  if (parsed.contains("anc") && parsed["anc"].is_string()) return parsed["anc"].get<std::string>();
  if (parsed.contains("resData") && parsed["resData"].is_object() &&
      parsed["resData"].contains("anc"))
    return parsed["resData"]["anc"].get<std::string>();
  fail("Could not find anc/resData.anc in response");
}

nlohmann::json decrypt_anc_text(const std::string& response_text,
                                const std::string& request_text,
                                const AncOptions& anc_options,
                                bool trace_enabled);

bool response_has_anc_text(const std::string& response_text) {
  std::string text = trim(response_text);
  if (text.empty()) return false;
  if (text[0] != '{') return true;
  nlohmann::json parsed = nlohmann::json::parse(text);
  return (parsed.contains("anc") && parsed["anc"].is_string()) ||
         (parsed.contains("resData") && parsed["resData"].is_object() &&
          parsed["resData"].contains("anc") && parsed["resData"]["anc"].is_string());
}

nlohmann::json unwrap_plain_getvinfo_json(const nlohmann::json& parsed) {
  if (parsed.contains("vl")) return parsed;
  if (parsed.contains("resData") && parsed["resData"].is_object()) {
    const auto& res = parsed["resData"];
    if (res.contains("vl")) return res;
  }
  fail("Response has no ANC payload and is not plaintext getvinfo JSON");
}

nlohmann::json decode_getvinfo_response_text(const std::string& response_text,
                                             const std::string& request_text,
                                             bool trace_enabled,
                                             const AncOptions& anc_options,
                                             bool* used_anc) {
  if (response_has_anc_text(response_text)) {
    if (used_anc) *used_anc = true;
    return decrypt_anc_text(response_text, request_text, anc_options, trace_enabled);
  }
  std::string text = trim(response_text);
  if (text.empty() || text[0] != '{')
    fail("Response is not JSON and has no ANC payload");
  if (used_anc) *used_anc = false;
  return unwrap_plain_getvinfo_json(nlohmann::json::parse(text));
}

int std_b64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

Bytes b64decode_loose(const std::string& value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (char c : value) {
    if (c == '-') normalized.push_back('+');
    else if (c == '_') normalized.push_back('/');
    else normalized.push_back(c);
  }
  Bytes out;
  out.reserve(normalized.size() / 4 * 3 + 2);
  uint32_t buf = 0;
  int bits = 0;
  for (char c : normalized) {
    int v = std_b64_value(c);
    if (v < 0) continue;
    buf = (buf << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((buf >> bits) & 0xff));
    }
  }
  return out;
}

Bytes apply_anc_algo(const nlohmann::json& entry, const Bytes& data, bool trace_enabled) {
  std::string algo = entry.value("algo_id", std::string());
  const auto& params = entry.at("algo_params");
  auto param = [&](size_t idx) -> Bytes {
    if (!params.is_array() || params.size() <= idx || !params[idx].is_string())
      fail(algo + " missing algo_params[" + std::to_string(idx) + "]");
    return bytes_from_ascii(params[idx].get<std::string>());
  };
  Bytes out;
  if (algo == "y0cii79j3m") out = repeating_xor(data, param(0));
  else if (algo == "m5h0zchrh5") out = aes_cbc_decrypt(data, param(0), param(1));
  else if (algo == "9x7k6uc7xw") out = chacha20_crypt_rfc(data, param(0), param(1), 0);
  else if (algo == "pplro0qj0r") out = rc4_crypt(data, param(0));
  else if (algo == "k7tslimq88") out = qq_tea_decrypt_anc(data, param(0));
  else if (algo == "o4stei6utn") out = des_cbc_decrypt(data, param(0), param(1));
  else fail("unsupported ANC algo_id: " + algo);
  trace(trace_enabled, "[algo] " + algo + ": in=" + std::to_string(data.size()) +
                           " out=" + std::to_string(out.size()));
  return out;
}

nlohmann::json decrypt_anc_text_with_material(const std::string& response_text,
                                              const std::string& request_text,
                                              const AncMaterial& material,
                                              bool trace_enabled) {
  trace(trace_enabled, "[anc] profile=" + material.profile);
  std::string anc_b64 = load_outer_anc_text(response_text);
  nlohmann::json inner =
      nlohmann::json::parse(ascii_from_bytes(b64decode_loose(anc_b64)));
  for (const char* key : {"rc", "aanc", "anc", "ufld"}) {
    if (!inner.contains(key)) fail(std::string("Inner ANC is missing key ") + key);
  }
  trace(trace_enabled, "[inner] rc=" + std::to_string(inner["rc"].get<std::string>().size()) +
                           " aanc=" + std::to_string(inner["aanc"].get<std::string>().size()) +
                           " anc=" + std::to_string(inner["anc"].get<std::string>().size()));

  Bytes rc_pre_key, rc_pre_iv;
  std::tie(rc_pre_key, rc_pre_iv) =
      derive_rc_prestage_from_ckey(request_text, material, trace_enabled);

  Bytes raw = b64decode_loose(inner["rc"].get<std::string>());
  std::vector<std::pair<std::string, Bytes>> candidates;
  if (raw.size() % 16 == 0) candidates.push_back({"raw", raw});
  if (!raw.empty() && (raw.size() - 1) % 16 == 0)
    candidates.push_back({"raw[:-1]", Bytes(raw.begin(), raw.end() - 1)});
  nlohmann::json rc_doc;
  bool rc_ok = false;
  for (const auto& cand : candidates) {
    try {
      Bytes stage2 = aes_cbc_decrypt(cand.second, rc_pre_key, rc_pre_iv);
      Bytes plain = rsa_oaep_sha1_decrypt(stage2, material.rsa_private_pem);
      rc_doc = nlohmann::json::parse(ascii_from_bytes(plain));
      trace(trace_enabled, "[rc] pre-stage AES/RSA ok via " + cand.first);
      rc_ok = true;
      break;
    } catch (const std::exception& e) {
      trace(trace_enabled, "[rc] candidate " + cand.first + " failed: " + e.what());
    }
  }
  if (!rc_ok) fail("rc decrypt did not succeed");

  if (!rc_doc.contains("algo_params") || !rc_doc["algo_params"].is_array() ||
      rc_doc["algo_params"].size() < 2)
    fail("rc JSON does not contain algo_params[0..1]");
  Bytes aanc_key = bytes_from_ascii(rc_doc["algo_params"][0].get<std::string>());
  Bytes aanc_iv = bytes_from_ascii(rc_doc["algo_params"][1].get<std::string>());
  Bytes aanc_plain = aes_cbc_decrypt(b64decode_loose(inner["aanc"].get<std::string>()),
                                     aanc_key, aanc_iv);
  nlohmann::json algo_doc = nlohmann::json::parse(ascii_from_bytes(aanc_plain));
  nlohmann::json algo_list =
      algo_doc.is_object() && algo_doc.contains("algo_list") ? algo_doc["algo_list"] : algo_doc;
  if (!algo_list.is_array() || algo_list.empty()) fail("algo_list is empty or invalid");

  Bytes payload = b64decode_loose(inner["anc"].get<std::string>());
  for (auto it = algo_list.rbegin(); it != algo_list.rend(); ++it) {
    payload = apply_anc_algo(*it, payload, trace_enabled);
  }
  return nlohmann::json::parse(ascii_from_bytes(payload));
}

nlohmann::json decrypt_anc_text(const std::string& response_text,
                                const std::string& request_text,
                                const AncOptions& anc_options,
                                bool trace_enabled) {
  std::vector<AncMaterial> materials =
      resolve_anc_materials(anc_options, request_text, trace_enabled);
  std::string last_error;
  for (size_t i = 0; i < materials.size(); ++i) {
    try {
      return decrypt_anc_text_with_material(response_text, request_text, materials[i],
                                            trace_enabled);
    } catch (const std::exception& e) {
      last_error = e.what();
      if (i + 1 >= materials.size()) break;
      trace(trace_enabled, "[anc] profile " + materials[i].profile +
                               " failed, trying " + materials[i + 1].profile +
                               ": " + last_error);
    }
  }
  fail("ANC decrypt did not succeed: " + last_error);
}

struct DrmMaterial {
  std::string key;
  std::string nonce;
  std::string nonce1;
};

bool find_first_ry(const nlohmann::json& node, std::string& out) {
  if (node.is_object()) {
    auto it = node.find("ry");
    if (it != node.end() && it->is_string() && !it->get<std::string>().empty()) {
      out = it->get<std::string>();
      return true;
    }
    for (const auto& item : node.items()) {
      if (find_first_ry(item.value(), out)) return true;
    }
  } else if (node.is_array()) {
    for (const auto& item : node) {
      if (find_first_ry(item, out)) return true;
    }
  }
  return false;
}

DrmMaterial decode_ry_material(const std::string& ry) {
  Bytes raw = b64decode_loose(ry);
  if (raw.size() < 9) fail("ry is too short");
  uint32_t payload_size = be32(&raw[4]);
  size_t take = std::min<size_t>(payload_size, raw.size() - 8);
  std::string payload(reinterpret_cast<const char*>(raw.data() + 8), take);
  nlohmann::json j = nlohmann::json::parse(ascii_from_bytes(b64decode_loose(payload)));
  DrmMaterial m;
  m.key = j.value("k1", std::string());
  m.nonce1 = j.value("nonce1", std::string());
  if (m.key.size() != 32) fail("ry.k1 must be 32 characters");
  if (m.nonce1.size() < 12) fail("ry.nonce1 must be at least 12 characters");
  m.nonce = m.nonce1.substr(0, 12);
  return m;
}

std::optional<DrmMaterial> maybe_extract_drm_material(const nlohmann::json& decoded) {
  std::string ry;
  if (!find_first_ry(decoded, ry)) return std::nullopt;
  return decode_ry_material(ry);
}

struct LocalMaterial {
  std::string key;
  std::string nonce;
  std::string keyid;
};

LocalMaterial derive_local_material(const std::string& sse_b64) {
  nlohmann::json info = nlohmann::json::parse(ascii_from_bytes(b64decode_loose(sse_b64)));
  std::string k = info.value("k", std::string());
  std::string key_store;
  if (k == "khym553sh0") key_store = "oGwG8CRiAMHQ7cWcZD_SQpoCvbAE8-NX";
  else if (k == "75uk6ytchx") key_store = "O/cH65h6bjEfoyWQzbFAu1t8NZ/Ns74n";
  else fail("No default KeyStore is available for k=" + k);
  std::string sha_input = info.value("a", std::string()) + info.value("p", std::string()) +
                          info.value("t", std::string()) + info.value("v", std::string());
  Bytes digest = sha256_bytes(sha_input);
  Bytes nonce12(12, 0);
  std::copy(digest.begin(), digest.begin() + 8, nonce12.begin() + 4);
  Bytes ct = b64decode_loose(info.at("f").get<std::string>());
  Bytes ks = chacha20_block_rfc(bytes_from_ascii(key_store), nonce12, 1);
  Bytes key(32);
  for (size_t i = 0; i < 32; ++i) key[i] = ct[i] ^ ks[i];
  return {ascii_from_bytes(key), info.value("n", std::string()), info.value("e", k)};
}

class TencentChacha20V3 {
 public:
  TencentChacha20V3(const Bytes& key, const Bytes& nonce) {
    if (key.size() != 32 || nonce.size() != 12) fail("Chacha20V3 key=32 nonce=12");
    uint32_t kw[8];
    uint32_t nw[3];
    for (int i = 0; i < 8; ++i) kw[i] = le32(&key[i * 4]);
    for (int i = 0; i < 3; ++i) nw[i] = le32(&nonce[i * 4]);
    base_ = {0x65278809u, 0x74857523u, 0x82626595u, kw[1], kw[2], kw[4], kw[7], kw[6],
             kw[5],       kw[3],       kw[0],       nw[1], nw[0], nw[2], 0,     0};
  }

  Bytes block(uint64_t counter64) const {
    const auto& s = base_;
    uint32_t counter_low = static_cast<uint32_t>(counter64);
    uint32_t counter_high = static_cast<uint32_t>(counter64 >> 32);
    uint32_t w14 = s[1], w12 = s[0];
    uint32_t w13 = s[4], w8 = s[5];
    uint32_t w16 = s[8], w15 = s[2];
    uint32_t w19 = s[7], w28 = s[10];
    uint32_t w9 = s[3];
    uint32_t w11 = counter_low;
    uint32_t w23 = s[6];
    uint32_t w24 = counter_high;
    uint32_t w25 = s[12];
    uint32_t w26 = s[13];
    uint32_t w27 = s[11];
    uint32_t w10 = s[9];
    uint32_t w20 = s[3];
    uint32_t w30 = s[9];
    (void)w9;
    (void)w10;
    for (int round = 0; round < 10; ++round) {
      w12 -= w13; w14 -= w15; uint32_t w3 = w20 - w11; w8 -= w19;
      w26 = w12 ^ w26; w25 = w14 ^ w25; w24 = w3 ^ w24; w23 = w8 ^ w23;
      uint32_t r26_16 = ror32(w26, 16), r25_16 = ror32(w25, 16);
      uint32_t r24_16 = ror32(w24, 16), r23_16 = ror32(w23, 16);
      w16 -= r26_16; uint32_t w17 = w28 - r25_16; uint32_t w0 = w30 - r24_16; uint32_t w5 = w27 - r23_16;
      w13 = w16 ^ w13; w15 = w17 ^ w15; w11 = w0 ^ w11; w19 = w5 ^ w19;
      uint32_t r13_20 = ror32(w13, 20), r15_20 = ror32(w15, 20);
      uint32_t r11_20 = ror32(w11, 20), r19_20 = ror32(w19, 20);
      w12 += r13_20; w14 += r15_20; w3 += r11_20; w8 += r19_20;
      uint32_t w21 = w12 ^ r26_16; w25 = w14 ^ r25_16; w24 = w3 ^ r24_16; w23 = w8 ^ r23_16;
      uint32_t r21_24 = ror32(w21, 24); uint32_t r25_24 = ror32(w25, 24);
      uint32_t r24_24 = ror32(w24, 24); uint32_t r23_24 = ror32(w23, 24);
      w16 += r21_24; w17 += r25_24; w0 += r24_24; w5 += r23_24;
      w13 = w16 ^ r13_20; w15 = w17 ^ r15_20; w11 = w0 ^ r11_20; w19 = w5 ^ r19_20;
      uint32_t r13_26 = ror32(w13, 26), r15_26 = ror32(w15, 26);
      uint32_t r11_26 = ror32(w11, 26), r19_26 = ror32(w19, 26);
      w12 -= r15_26; w14 -= r11_26; w3 -= r19_26; w8 -= r13_26;
      w23 = w12 ^ r23_24; w21 = w14 ^ r21_24; w25 = w3 ^ r25_24; w24 = w8 ^ r24_24;
      r23_16 = ror32(w23, 16); uint32_t r21_16 = ror32(w21, 16);
      r25_16 = ror32(w25, 16); r24_16 = ror32(w24, 16);
      w0 -= r23_16; w5 -= r21_16; w16 -= r25_16; w17 -= r24_16;
      w15 = w0 ^ r15_26; w11 = w5 ^ r11_26; w19 = w16 ^ r19_26; w13 = w17 ^ r13_26;
      r15_20 = ror32(w15, 20); r11_20 = ror32(w11, 20); r19_20 = ror32(w19, 20); r13_20 = ror32(w13, 20);
      w12 += r15_20; w14 += r11_20; w20 = w3 + r19_20; w8 += r13_20;
      w23 = w12 ^ r23_16; w21 = w14 ^ r21_16; w25 = w20 ^ r25_16; w24 = w8 ^ r24_16;
      r23_24 = ror32(w23, 24); r21_24 = ror32(w21, 24); r25_24 = ror32(w25, 24); r24_24 = ror32(w24, 24);
      w23 = r23_24; w26 = r21_24; w25 = r25_24; w24 = r24_24;
      w30 = w23 + w0; w27 = w26 + w5; w16 = w25 + w16; w28 = w24 + w17;
      w15 = w30 ^ r15_20; w11 = w27 ^ r11_20; w17 = w16 ^ r19_20; w13 = w28 ^ r13_20;
      w15 = ror32(w15, 26); w11 = ror32(w11, 26); w19 = ror32(w17, 26); w13 = ror32(w13, 26);
    }
    std::array<uint32_t, 16> words{
        w12 + s[0], w14 + s[1], w20 + s[3], w8 + s[5],
        w13 + s[4], w15 + s[2], w11 + counter_low, w19 + s[7],
        w16 + s[8], w28 + s[10], w30 + s[9], w27 + s[11],
        w26 + s[13], w25 + s[12], w24 + counter_high, w23 + s[6]};
    Bytes out;
    out.reserve(64);
    for (uint32_t v : words) put_le32(out, v);
    return out;
  }

  Bytes keystream(size_t length, uint64_t absolute_offset = 0) const {
    Bytes out(length);
    uint64_t counter = absolute_offset >> 6;
    size_t skip = absolute_offset & 0x3f;
    size_t pos = 0;
    while (pos < length) {
      Bytes stream = block(counter++);
      size_t take = std::min<size_t>(length - pos, 64 - skip);
      std::copy(stream.begin() + static_cast<std::ptrdiff_t>(skip),
                stream.begin() + static_cast<std::ptrdiff_t>(skip + take), out.begin() + pos);
      pos += take;
      skip = 0;
    }
    return out;
  }

 private:
 std::array<uint32_t, 16> base_{};
};

bool is_ts(const Bytes& data);
bool is_mp4_like(const Bytes& data);

std::optional<LocalMaterial> local_material_from_video(const nlohmann::json& video) {
  if (!video.contains("sse") || !video["sse"].is_string() ||
      video["sse"].get<std::string>().empty())
    return std::nullopt;
  return derive_local_material(video["sse"].get<std::string>());
}

bool normalize_local_transport(Bytes& data,
                               const std::optional<LocalMaterial>& material) {
  if (is_ts(data) || is_mp4_like(data)) return false;
  if (!material)
    fail("Segment is not MPEG-TS/fMP4 and the response has no local key");
  Bytes key = bytes_from_ascii(material->key);
  Bytes raw_nonce = bytes_from_ascii(material->nonce);
  if (raw_nonce.size() != 12 && raw_nonce.size() != 32)
    fail("Local nonce must be 12 or 32 bytes");
  Bytes nonce(raw_nonce.begin(), raw_nonce.begin() + 12);
  TencentChacha20V3 cipher(key, nonce);
  Bytes keystream = cipher.keystream(data.size());
  for (size_t i = 0; i < data.size(); ++i) data[i] ^= keystream[i];
  if (!is_ts(data) && !is_mp4_like(data)) {
    size_t head_len = std::min<size_t>(data.size(), 16);
    fail("Local transform completed, but the segment is still not MPEG-TS/fMP4; head=" +
         util::hex_encode(Bytes(data.begin(), data.begin() + head_len)));
  }
  return true;
}

struct Segment {
  int index = 0;
  std::string uri;
  std::string url;
  std::string filename;
  double duration_seconds = 0.0;
  bool init_map = false;
};

struct SegmentEntry {
  std::string uri;
  double duration_seconds = 0.0;
};

std::vector<SegmentEntry> segment_entries(const std::string& m3u8) {
  std::vector<SegmentEntry> out;
  std::istringstream ss(m3u8);
  std::string line;
  double pending_duration = 0.0;
  while (std::getline(ss, line)) {
    std::string s = trim(line);
    if (s.empty()) continue;
    if (s.rfind("#EXTINF:", 0) == 0) {
      std::string value = s.substr(8);
      auto comma = value.find(',');
      if (comma != std::string::npos) value.resize(comma);
      try {
        pending_duration = std::stod(value);
      } catch (...) {
        pending_duration = 0.0;
      }
      continue;
    }
    if (s[0] == '#') continue;
    out.push_back({s, pending_duration});
    pending_duration = 0.0;
  }
  return out;
}

std::vector<std::string> segment_uris(const std::string& m3u8) {
  std::vector<std::string> out;
  for (const auto& entry : segment_entries(m3u8)) out.push_back(entry.uri);
  return out;
}

std::optional<std::string> map_uri(const std::string& m3u8) {
  std::istringstream ss(m3u8);
  std::string line;
  while (std::getline(ss, line)) {
    std::string s = trim(line);
    if (s.rfind("#EXT-X-MAP:", 0) != 0) continue;
    std::string key = "URI=\"";
    auto pos = s.find(key);
    if (pos == std::string::npos) continue;
    pos += key.size();
    auto end = s.find('"', pos);
    if (end == std::string::npos || end <= pos) continue;
    return s.substr(pos, end - pos);
  }
  return std::nullopt;
}

std::string basename_from_uri(const std::string& uri, int index) {
  std::string path = uri.substr(0, uri.find('?'));
  auto slash = path.find_last_of('/');
  std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
  if (base.empty()) {
    std::ostringstream os;
    os << std::setw(4) << std::setfill('0') << index << ".ts";
    return os.str();
  }
  return base;
}

std::string url_join_simple(const std::string& base, const std::string& uri) {
  if (uri.rfind("http://", 0) == 0 || uri.rfind("https://", 0) == 0) return uri;
  if (base.empty()) return uri;
  auto q = base.find('?');
  std::string clean = q == std::string::npos ? base : base.substr(0, q);
  auto slash = clean.find_last_of('/');
  std::string prefix = slash == std::string::npos ? clean + "/" : clean.substr(0, slash + 1);
  return prefix + uri;
}

bool query_has_key(const std::string& url, const std::string& key) {
  auto q = url.find('?');
  if (q == std::string::npos) return false;
  std::string needle = key + "=";
  for (std::size_t pos = q + 1; pos < url.size();) {
    std::size_t end = url.find('&', pos);
    if (end == std::string::npos) end = url.size();
    if (url.compare(pos, needle.size(), needle) == 0) return true;
    pos = end + 1;
  }
  return false;
}

void append_query_param(std::string& url, const std::string& key, const std::string& value) {
  if (query_has_key(url, key)) return;
  char sep = url.find('?') == std::string::npos ? '?' : '&';
  url.push_back(sep);
  url.append(key);
  url.push_back('=');
  url.append(value);
}

std::string abbreviate_url_for_error(const std::string& url) {
  constexpr size_t kMax = 320;
  if (url.size() <= kMax) return url;
  auto q = url.find('?');
  std::string head = q == std::string::npos ? url.substr(0, kMax) : url.substr(0, q);
  if (head.size() > kMax - 16) {
    head.resize(kMax - 16);
    head += "...";
  }
  if (q != std::string::npos) head += "?...";
  return head;
}

std::vector<Segment> build_segments(const std::string& m3u8, const std::string& base_url,
                                    int video_feature, int cdn_strategy,
                                    int emergency_time_ms, bool add_segment_query) {
  std::vector<Segment> out;
  if (auto init_uri = map_uri(m3u8)) {
    std::string url = url_join_simple(base_url, *init_uri);
    if (add_segment_query)
      url = normalize_segment_url(std::move(url), video_feature, cdn_strategy,
                                  emergency_time_ms);
    out.push_back({-1, *init_uri, std::move(url),
                   "init_" + basename_from_uri(*init_uri, 0), 0.0, true});
  }
  for (const auto& entry : segment_entries(m3u8)) {
    int idx = static_cast<int>(out.size());
    if (!out.empty() && out.front().init_map) --idx;
    std::string url = url_join_simple(base_url, entry.uri);
    if (add_segment_query)
      url = normalize_segment_url(std::move(url), video_feature, cdn_strategy,
                                  emergency_time_ms);
    std::ostringstream name;
    name << std::setw(4) << std::setfill('0') << idx << "_"
         << basename_from_uri(entry.uri, idx);
    out.push_back({idx, entry.uri, std::move(url), name.str(),
                   entry.duration_seconds, false});
  }
  if (out.empty()) fail("Playlist contains no segments");
  return out;
}

std::string first_base_url_from_video(const nlohmann::json& video) {
  if (video.contains("ul") && video["ul"].is_object() &&
      video["ul"].contains("ui") && video["ul"]["ui"].is_array() &&
      !video["ul"]["ui"].empty())
    return video["ul"]["ui"][0].value("url", std::string());
  return {};
}

std::string first_hls_path_from_video(const nlohmann::json& video) {
  if (!video.contains("ul") || !video["ul"].is_object() ||
      !video["ul"].contains("ui") || !video["ul"]["ui"].is_array())
    return {};
  for (const auto& ui : video["ul"]["ui"]) {
    if (!ui.is_object() || !ui.contains("hls") || !ui["hls"].is_object()) continue;
    std::string path = ui["hls"].value("pt", std::string());
    if (!path.empty()) return path;
  }
  return {};
}

std::string first_hls_playlist_url_from_video(const nlohmann::json& video) {
  if (!video.contains("ul") || !video["ul"].is_object() ||
      !video["ul"].contains("ui") || !video["ul"]["ui"].is_array())
    return {};
  for (const auto& ui : video["ul"]["ui"]) {
    if (!ui.is_object()) continue;
    std::string base = ui.value("url", std::string());
    if (base.empty() || !ui.contains("hls") || !ui["hls"].is_object()) continue;
    std::string path = ui["hls"].value("pt", std::string());
    if (!path.empty()) return url_join_simple(base, path);
  }
  return {};
}

struct SubtitleTrack {
  int index = 0;
  int id = 0;
  std::string lang;
  std::string name;
  std::string url;
  std::vector<std::string> urls;
};

std::vector<SubtitleTrack> subtitle_tracks_from_sfl(const nlohmann::json& decoded) {
  // Tencent getvinfo response sample:
  // ~/.tencent/videos/f4102c88kzn/fhd/decoded.json has sfl.fi[].url and
  // sfl.fi[].urlList.ui[].url for WebVTT HLS subtitle playlists.
  std::vector<SubtitleTrack> tracks;
  if (!decoded.contains("sfl") || !decoded["sfl"].is_object()) return tracks;
  const auto& sfl = decoded["sfl"];
  if (!sfl.contains("fi") || !sfl["fi"].is_array()) return tracks;
  for (const auto& item : sfl["fi"]) {
    if (!item.is_object()) continue;
    SubtitleTrack track;
    track.index = static_cast<int>(tracks.size());
    track.id = item.value("id", 0);
    track.lang = item.value("lang", std::string());
    track.name = item.value("name", std::string());
    if (item.contains("url") && item["url"].is_string()) {
      track.url = item["url"].get<std::string>();
      if (!track.url.empty()) track.urls.push_back(track.url);
    }
    if (item.contains("urlList") && item["urlList"].is_object() &&
        item["urlList"].contains("ui") && item["urlList"]["ui"].is_array()) {
      for (const auto& ui : item["urlList"]["ui"]) {
        std::string url = ui.value("url", std::string());
        if (url.empty()) continue;
        if (std::find(track.urls.begin(), track.urls.end(), url) == track.urls.end())
          track.urls.push_back(url);
      }
    }
    if (track.url.empty() && !track.urls.empty()) track.url = track.urls.front();
    if (!track.url.empty()) tracks.push_back(std::move(track));
  }
  return tracks;
}

std::string subtitle_component(const SubtitleTrack& track) {
  std::string value = track.lang.empty() ? track.name : track.lang;
  if (value.empty() && track.id > 0) value = std::to_string(track.id);
  if (value.empty()) value = std::to_string(track.index);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  for (char& c : value) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (!std::isalnum(uc) && c != '.' && c != '_' && c != '-') c = '_';
  }
  if (value.empty() || value == "." || value == "..")
    value = "subtitle_" + std::to_string(track.index);
  return value;
}

std::string normalize_subtitle_format(std::string format) {
  std::transform(format.begin(), format.end(), format.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (format != "vtt" && format != "srt" && format != "ass")
    fail("Unsupported subtitle format: " + format + " (expected vtt/srt/ass)");
  return format;
}

fs::path subtitle_output_path(const fs::path& media_output, const SubtitleTrack& track,
                              size_t track_count, const std::string& format) {
  std::string suffix = subtitle_component(track);
  fs::path parent = media_output.parent_path();
  std::string stem = media_output.stem().string();
  if (stem.empty()) stem = "subtitle";
  std::ostringstream name;
  name << stem << "." << suffix;
  if (track_count > 1 && track.id > 0) name << "." << track.id;
  name << "." << format;
  return parent / name.str();
}

std::string merge_webvtt_segments(const std::vector<std::string>& segments) {
  // HLS WebVTT segments each may carry their own WEBVTT/X-TIMESTAMP-MAP header.
  // A standalone .vtt keeps one WEBVTT header, then appends cue payload blocks.
  std::ostringstream out;
  bool wrote_header = false;
  bool wrote_payload = false;
  for (const std::string& segment : segments) {
    std::istringstream ss(segment);
    std::string line;
    bool at_start = true;
    while (std::getline(ss, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      std::string stripped = trim(line);
      if (at_start && stripped.rfind("\xEF\xBB\xBF", 0) == 0)
        stripped.erase(0, 3);
      if (at_start && stripped.rfind("WEBVTT", 0) == 0) {
        if (!wrote_header) {
          out << "WEBVTT\n\n";
          wrote_header = true;
        }
        at_start = false;
        continue;
      }
      at_start = false;
      if (stripped.rfind("X-TIMESTAMP-MAP=", 0) == 0) continue;
      if (!wrote_header && !stripped.empty()) {
        out << "WEBVTT\n\n";
        wrote_header = true;
      }
      if (!stripped.empty() || wrote_payload) {
        out << line << "\n";
        wrote_payload = true;
      }
    }
    if (wrote_payload) out << "\n";
  }
  if (!wrote_header) out << "WEBVTT\n";
  return out.str();
}

std::string decode_basic_entities(std::string text) {
  auto replace_all = [&](const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  replace_all("&amp;", "&");
  replace_all("&lt;", "<");
  replace_all("&gt;", ">");
  replace_all("&quot;", "\"");
  replace_all("&#39;", "'");
  replace_all("&nbsp;", " ");
  return text;
}

std::string strip_webvtt_tags(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  bool in_tag = false;
  for (char c : text) {
    if (c == '<') {
      in_tag = true;
      continue;
    }
    if (in_tag) {
      if (c == '>') in_tag = false;
      continue;
    }
    out.push_back(c);
  }
  return decode_basic_entities(out);
}

struct SubtitleCue {
  int64_t start_ms = 0;
  int64_t end_ms = 0;
  std::vector<std::string> text;
};

std::optional<int64_t> parse_webvtt_time_ms(const std::string& raw) {
  std::regex rx(R"(^\s*(?:(\d+):)?(\d{2}):(\d{2})[\.,](\d{3})\s*$)");
  std::smatch m;
  if (!std::regex_match(raw, m, rx)) return std::nullopt;
  int64_t hours = m[1].matched ? std::stoll(m[1].str()) : 0;
  int64_t minutes = std::stoll(m[2].str());
  int64_t seconds = std::stoll(m[3].str());
  int64_t millis = std::stoll(m[4].str());
  return ((hours * 60 + minutes) * 60 + seconds) * 1000 + millis;
}

std::optional<SubtitleCue> parse_webvtt_timing_line(const std::string& line) {
  size_t arrow = line.find("-->");
  if (arrow == std::string::npos) return std::nullopt;
  std::string start = trim(line.substr(0, arrow));
  std::string end_and_settings = trim(line.substr(arrow + 3));
  size_t ws = end_and_settings.find_first_of(" \t");
  std::string end = ws == std::string::npos ? end_and_settings : end_and_settings.substr(0, ws);
  auto start_ms = parse_webvtt_time_ms(start);
  auto end_ms = parse_webvtt_time_ms(end);
  if (!start_ms || !end_ms) return std::nullopt;
  SubtitleCue cue;
  cue.start_ms = *start_ms;
  cue.end_ms = *end_ms;
  return cue;
}

std::vector<SubtitleCue> parse_webvtt_cues(const std::string& webvtt) {
  std::vector<std::string> lines;
  std::istringstream ss(webvtt);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }

  std::vector<SubtitleCue> cues;
  for (size_t i = 0; i < lines.size();) {
    std::string current = trim(lines[i]);
    if (current.empty() || current.rfind("WEBVTT", 0) == 0 ||
        current.rfind("X-TIMESTAMP-MAP=", 0) == 0) {
      ++i;
      continue;
    }
    if (current.rfind("NOTE", 0) == 0 || current == "STYLE" || current == "REGION") {
      while (i < lines.size() && !trim(lines[i]).empty()) ++i;
      continue;
    }

    std::optional<SubtitleCue> cue = parse_webvtt_timing_line(lines[i]);
    if (!cue && i + 1 < lines.size())
      cue = parse_webvtt_timing_line(lines[i + 1]);
    if (!cue) {
      ++i;
      continue;
    }
    i += lines[i].find("-->") != std::string::npos ? 1 : 2;
    while (i < lines.size() && !trim(lines[i]).empty()) {
      cue->text.push_back(strip_webvtt_tags(lines[i]));
      ++i;
    }
    if (!cue->text.empty()) cues.push_back(std::move(*cue));
  }
  return cues;
}

std::string srt_time(int64_t ms) {
  ms = std::max<int64_t>(0, ms);
  int64_t hours = ms / 3600000;
  ms %= 3600000;
  int64_t minutes = ms / 60000;
  ms %= 60000;
  int64_t seconds = ms / 1000;
  int64_t millis = ms % 1000;
  std::ostringstream out;
  out << std::setw(2) << std::setfill('0') << hours << ":"
      << std::setw(2) << minutes << ":"
      << std::setw(2) << seconds << ","
      << std::setw(3) << millis;
  return out.str();
}

std::string ass_time(int64_t ms) {
  ms = std::max<int64_t>(0, ms);
  int64_t centiseconds = (ms + 5) / 10;
  int64_t hours = centiseconds / 360000;
  centiseconds %= 360000;
  int64_t minutes = centiseconds / 6000;
  centiseconds %= 6000;
  int64_t seconds = centiseconds / 100;
  centiseconds %= 100;
  std::ostringstream out;
  out << hours << ":"
      << std::setw(2) << std::setfill('0') << minutes << ":"
      << std::setw(2) << seconds << "."
      << std::setw(2) << centiseconds;
  return out.str();
}

std::string ass_text_line(std::string text) {
  for (char& c : text) {
    if (c == '{' || c == '}') c = ' ';
  }
  return text;
}

std::string convert_webvtt_to_srt(const std::string& webvtt) {
  std::vector<SubtitleCue> cues = parse_webvtt_cues(webvtt);
  std::ostringstream out;
  for (size_t i = 0; i < cues.size(); ++i) {
    out << (i + 1) << "\n"
        << srt_time(cues[i].start_ms) << " --> " << srt_time(cues[i].end_ms) << "\n";
    for (const std::string& text : cues[i].text) out << text << "\n";
    out << "\n";
  }
  return out.str();
}

std::string convert_webvtt_to_ass(const std::string& webvtt) {
  std::vector<SubtitleCue> cues = parse_webvtt_cues(webvtt);
  std::ostringstream out;
  out << "[Script Info]\n"
      << "ScriptType: v4.00+\n"
      << "WrapStyle: 0\n"
      << "ScaledBorderAndShadow: yes\n\n"
      << "[V4+ Styles]\n"
      << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
         "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
         "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
         "MarginL, MarginR, MarginV, Encoding\n"
      << "Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,"
         "0,0,0,0,100,100,0,0,1,2,0,2,30,30,30,1\n\n"
      << "[Events]\n"
      << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, "
         "Effect, Text\n";
  for (const SubtitleCue& cue : cues) {
    std::ostringstream text;
    for (size_t i = 0; i < cue.text.size(); ++i) {
      if (i > 0) text << "\\N";
      text << ass_text_line(cue.text[i]);
    }
    out << "Dialogue: 0," << ass_time(cue.start_ms) << "," << ass_time(cue.end_ms)
        << ",Default,,0,0,0,," << text.str() << "\n";
  }
  return out.str();
}

std::string convert_subtitle_format(const std::string& webvtt, const std::string& format) {
  std::string normalized = normalize_subtitle_format(format);
  if (normalized == "vtt") return webvtt;
  if (normalized == "srt") return convert_webvtt_to_srt(webvtt);
  return convert_webvtt_to_ass(webvtt);
}

Bytes http_get_with_retries(const std::string& url,
                            const std::vector<std::pair<std::string, std::string>>& headers,
                            long timeout, int retries,
                            std::atomic<uint64_t>* progress_bytes = nullptr) {
  std::string last_error;
  for (int attempt = 0; attempt <= retries; ++attempt) {
    try {
      return http_get_bytes(url, headers, timeout, progress_bytes);
    } catch (const std::exception& e) {
      last_error = e.what();
      if (attempt < retries)
        std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
    }
  }
  fail(last_error.empty() ? "subtitle GET failed" : last_error);
}

void download_subtitles(const nlohmann::json& decoded, const std::string& request_text,
                        const fs::path& work_dir, const fs::path& media_output,
                        const std::string& download_user_agent, long timeout, int retries,
                        const std::string& subtitle_format, bool trace_enabled) {
  std::string format = normalize_subtitle_format(subtitle_format);
  std::vector<SubtitleTrack> tracks = subtitle_tracks_from_sfl(decoded);
  if (tracks.empty()) {
    log_out() << "subtitle=none\n";
    return;
  }
  fs::path root = work_dir / "subtitles";
  fs::create_directories(root);
  auto headers = request_headers_for_segments(request_text, download_user_agent);
  std::vector<std::pair<std::string, std::string>> segment_headers;
  for (const auto& header : headers) {
    std::string key = header.first;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key == "cookie") continue;
    segment_headers.push_back(header);
  }

  log_out() << "subtitle_tracks=" << tracks.size() << "\n";
  for (const SubtitleTrack& track : tracks) {
    std::string component = subtitle_component(track);
    fs::path track_dir = root / component;
    fs::create_directories(track_dir / "segments");
    write_text(track_dir / "track.json",
               nlohmann::json{{"id", track.id},
                              {"lang", track.lang},
                              {"name", track.name},
                              {"url", track.url},
                              {"urls", track.urls}}
                   .dump(2) +
                   "\n");
    fs::path final_path = subtitle_output_path(media_output, track, tracks.size(), format);
    std::string last_error;
    bool done = false;
    for (const std::string& playlist_url : track.urls) {
      try {
        Bytes first = http_get_with_retries(playlist_url, headers, timeout, retries);
        std::string first_text(first.begin(), first.end());
        if (first_text.find("#EXTM3U") == std::string::npos) {
          write_text(track_dir / "subtitle.vtt", first_text);
          std::string converted = convert_subtitle_format(first_text, format);
          write_text(track_dir / ("subtitle." + format), converted);
          write_text(final_path, converted);
          done = true;
          break;
        }
        write_text(track_dir / "source.m3u8", first_text);
        std::vector<SegmentEntry> entries = segment_entries(first_text);
        if (entries.empty()) fail("Subtitle playlist contains no segments");
        std::vector<std::string> parts;
        for (size_t i = 0; i < entries.size(); ++i) {
          std::string url = url_join_simple(playlist_url, entries[i].uri);
          fs::path segment_path = track_dir / "segments" / basename_from_uri(entries[i].uri,
                                                                             static_cast<int>(i));
          std::string text;
          if (file_size_or_zero(segment_path) > 0) {
            Bytes cached = read_bytes(segment_path);
            text.assign(cached.begin(), cached.end());
          } else {
            Bytes data = http_get_with_retries(url, segment_headers, timeout, retries);
            write_bytes(segment_path, data);
            text.assign(data.begin(), data.end());
          }
          parts.push_back(std::move(text));
        }
        std::string merged = merge_webvtt_segments(parts);
        write_text(track_dir / "subtitle.vtt", merged);
        std::string converted = convert_subtitle_format(merged, format);
        write_text(track_dir / ("subtitle." + format), converted);
        write_text(final_path, converted);
        done = true;
        break;
      } catch (const std::exception& e) {
        last_error = e.what();
        if (trace_enabled)
          log_err() << "subtitle url failed: " << abbreviate_url_for_error(playlist_url)
                    << " error=" << last_error << "\n";
      }
    }
    if (!done) fail("subtitle " + component + " failed: " + last_error);
    log_out() << "subtitle[" << track.index << "] lang=" << track.lang
              << " name=" << track.name
              << " format=" << format
              << " output=" << final_path << "\n";
  }
}

void print_no_download_info(const nlohmann::json& decoded, const nlohmann::json& video,
                            const CacheIdentity& request_identity, bool used_anc,
                            int video_index) {
  std::string m3u8;
  if (video.contains("ul") && video["ul"].is_object())
    m3u8 = video["ul"].value("m3u8", std::string());
  std::vector<std::string> uris = segment_uris(m3u8);
  size_t getplay_count = 0;
  for (const auto& uri : uris) {
    if (uri.find("/getPlayTs/") != std::string::npos) ++getplay_count;
  }
  bool has_init_map = map_uri(m3u8).has_value();
  std::string base_url = first_base_url_from_video(video);
  std::string hls_path = first_hls_path_from_video(video);
  std::string hls_playlist_url = first_hls_playlist_url_from_video(video);

  log_out() << "# no-download info\n"
            << "response_payload=" << (used_anc ? "anc" : "plaintext") << "\n"
            << "em=" << decoded.value("em", -1)
            << " exem=" << decoded.value("exem", 0);
  std::string msg = decoded.value("msg", std::string());
  if (!msg.empty()) log_out() << " msg=" << msg;
  log_out() << "\n";
  if (decoded.contains("login")) log_out() << "login=" << decoded.value("login", 0) << "\n";

  log_out() << "\n# request\n"
            << "vid=" << request_identity.vid << "\n"
            << "defn=" << request_identity.defn << "\n";

  log_out() << "\n# video\n"
            << "video_index=" << video_index << "\n"
            << "vid=" << video.value("vid", request_identity.vid) << "\n"
            << "title=" << video.value("ti", std::string()) << "\n"
            << "filename=" << video.value("fn", std::string()) << "\n"
            << "drm=" << video.value("drm", 0) << "\n"
            << "feature=" << video.value("feature", 0) << "\n"
            << "type=" << video.value("type", 0) << "\n"
            << "br=" << video.value("br", 0) << "\n"
            << "fs=" << video.value("fs", 0) << "\n";
  if (video.contains("keyid") && video["keyid"].is_string())
    log_out() << "keyid=" << video.value("keyid", std::string()) << "\n";

  log_out() << "\n# playlist\n"
            << "embedded_m3u8=" << (m3u8.empty() ? "no" : "yes") << "\n"
            << "segment_count=" << uris.size() << "\n"
            << "getplayts_count=" << getplay_count << "\n"
            << "has_init_map=" << (has_init_map ? "yes" : "no") << "\n";
  if (!base_url.empty()) log_out() << "base_url=" << base_url << "\n";
  if (!hls_path.empty()) log_out() << "hls_path=" << hls_path << "\n";
  if (!hls_playlist_url.empty()) log_out() << "hls_playlist_url=" << hls_playlist_url << "\n";

  log_out() << "\n# drm\n";
  if (auto drm = maybe_extract_drm_material(decoded)) {
    log_out() << "k1=" << drm->key << "\n"
              << "nonce1=" << drm->nonce1 << "\n"
              << "nonce=" << drm->nonce << "\n";
  } else {
    log_out() << "material=none\n";
  }

  log_out() << "\n# local_transport\n";
  if (auto local = local_material_from_video(video)) {
    log_out() << "key=" << local->key << "\n"
              << "nonce=" << local->nonce << "\n"
              << "keyid=" << local->keyid << "\n";
  } else {
    log_out() << "material=none\n";
  }
}

void write_local_playlist(const std::string& m3u8, const std::vector<Segment>& segments,
                          const fs::path& out_path) {
  std::map<std::string, std::string> mapped;
  for (const auto& seg : segments) mapped[seg.uri] = "segments/" + seg.filename;
  std::ostringstream out;
  std::istringstream ss(m3u8);
  std::string line;
  while (std::getline(ss, line)) {
    std::string stripped = trim(line);
    if (stripped.rfind("#EXT-X-MAP:", 0) == 0) {
      std::string key = "URI=\"";
      auto pos = line.find(key);
      if (pos != std::string::npos) {
        auto value_pos = pos + key.size();
        auto end = line.find('"', value_pos);
        if (end != std::string::npos) {
          auto it = mapped.find(line.substr(value_pos, end - value_pos));
          if (it != mapped.end())
            line.replace(value_pos, end - value_pos, it->second);
        }
      }
    }
    auto it = mapped.find(stripped);
    out << (it == mapped.end() ? line : it->second) << "\n";
  }
  write_text(out_path, out.str());
}

struct CurlGlobal {
  CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
  ~CurlGlobal() { curl_global_cleanup(); }
};

struct CurlWriteContext {
  Bytes* out = nullptr;
  std::atomic<uint64_t>* progress_bytes = nullptr;
};

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* ctx = static_cast<CurlWriteContext*>(userdata);
  size_t n = size * nmemb;
  ctx->out->insert(ctx->out->end(), reinterpret_cast<uint8_t*>(ptr),
                   reinterpret_cast<uint8_t*>(ptr) + n);
  if (ctx->progress_bytes) ctx->progress_bytes->fetch_add(n, std::memory_order_relaxed);
  return n;
}

Bytes http_get_bytes_once(const std::string& url,
                          const std::vector<std::pair<std::string, std::string>>& headers,
                          long timeout, std::atomic<uint64_t>* progress_bytes,
                          bool ignore_content_length) {
  static CurlGlobal curl_global;
  (void)curl_global;
  CURL* raw = curl_easy_init();
  if (!raw) fail("curl_easy_init failed");
  std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(raw, curl_easy_cleanup);
  Bytes body;
  CurlWriteContext write_ctx{&body, progress_bytes};
  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, timeout);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &write_ctx);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
  // Tencent CDN may advertise a larger Content-Length than it actually sends
  // for some transformed TS URLs; the app accepts the close-delimited body.
  if (ignore_content_length) curl_easy_setopt(curl.get(), CURLOPT_IGNORE_CONTENT_LENGTH, 1L);
  struct curl_slist* raw_headers = nullptr;
  for (const auto& kv : headers) {
    std::string line = kv.first + ": " + kv.second;
    raw_headers = curl_slist_append(raw_headers, line.c_str());
  }
  if (ignore_content_length) raw_headers = curl_slist_append(raw_headers, "Connection: close");
  std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> hdrs(raw_headers, curl_slist_free_all);
  if (hdrs) curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, hdrs.get());
  CURLcode rc = curl_easy_perform(curl.get());
  if (rc != CURLE_OK) fail(std::string("curl GET failed: ") + curl_easy_strerror(rc));
  long status = 0;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
  if (status < 200 || status >= 300) {
    std::string detail(body.begin(), body.end());
    if (detail.size() > 240) detail.resize(240);
    detail = trim(detail);
    fail("HTTP status " + std::to_string(status) +
         (detail.empty() ? std::string() : " (" + detail + ")") + " for " + url);
  }
  return body;
}

Bytes http_get_bytes(const std::string& url, const std::vector<std::pair<std::string, std::string>>& headers,
                     long timeout, std::atomic<uint64_t>* progress_bytes = nullptr) {
  try {
    return http_get_bytes_once(url, headers, timeout, progress_bytes, false);
  } catch (const std::exception& e) {
    std::string msg = e.what();
    if (msg.find("Transferred a partial file") == std::string::npos) throw;
    return http_get_bytes_once(url, headers, timeout, progress_bytes, true);
  }
}

void parallel_for(size_t count, int workers, const std::function<void(size_t)>& fn) {
  workers = std::max(1, workers);
  std::atomic<size_t> next{0};
  std::exception_ptr first_error = nullptr;
  std::mutex error_mu;
  std::vector<std::thread> threads;
  for (int w = 0; w < workers; ++w) {
    threads.emplace_back([&] {
      while (true) {
        size_t i = next.fetch_add(1);
        if (i >= count) break;
        {
          std::lock_guard<std::mutex> lock(error_mu);
          if (first_error) break;
        }
        try {
          fn(i);
        } catch (...) {
          std::lock_guard<std::mutex> lock(error_mu);
          if (!first_error) first_error = std::current_exception();
          break;
        }
      }
    });
  }
  for (auto& t : threads) t.join();
  if (first_error) std::rethrow_exception(first_error);
}

std::string human_bytes(double value) {
  static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  std::ostringstream out;
  if (unit == 0)
    out << static_cast<uint64_t>(value) << units[unit];
  else
    out << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1)
        << value << units[unit];
  return out.str();
}

class DownloadProgress {
 public:
  DownloadProgress(std::atomic<int>& done, std::atomic<uint64_t>& bytes,
                   size_t total_segments)
      : done_(done), bytes_(bytes), total_(total_segments),
        start_(std::chrono::steady_clock::now()) {
    worker_ = std::thread([this] { run(); });
  }

  ~DownloadProgress() { stop(); }

  void stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) return;
    if (worker_.joinable()) worker_.join();
    print(true);
    log_err() << "\n";
  }

 private:
  void run() {
    while (!stopped_.load()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if (!stopped_.load()) print(false);
    }
  }

  void print(bool final) {
    auto now = std::chrono::steady_clock::now();
    uint64_t cur_bytes = bytes_.load(std::memory_order_relaxed);
    auto last_time = last_time_;
    uint64_t last_bytes = last_bytes_;
    double interval =
        std::chrono::duration<double>(now - last_time).count();
    double elapsed =
        std::chrono::duration<double>(now - start_).count();
    double speed = interval > 0.0 ? (cur_bytes - last_bytes) / interval : 0.0;
    double avg = elapsed > 0.0 ? cur_bytes / elapsed : 0.0;
    last_time_ = now;
    last_bytes_ = cur_bytes;
    // Dòng tiến độ ghi đè chính nó: gom thành MỘT thông điệp rồi gửi qua
    // log_progress (giao diện tự quyết cách hiển thị — CLI dùng '\r', GUI cập
    // nhật thanh tiến độ). Không dùng LogStream vì thông điệp này không có '\n'.
    std::ostringstream progress;
    progress << "download " << done_.load() << "/" << total_ << " "
             << human_bytes(static_cast<double>(cur_bytes))
             << " at " << human_bytes(speed) << "/s"
             << " avg " << human_bytes(avg) << "/s";
    if (final) progress << " done";
    log_progress(progress.str());
  }

  std::atomic<int>& done_;
  std::atomic<uint64_t>& bytes_;
  size_t total_ = 0;
  std::chrono::steady_clock::time_point start_;
  std::chrono::steady_clock::time_point last_time_ = start_;
  uint64_t last_bytes_ = 0;
  std::atomic<bool> stopped_{false};
  std::thread worker_;
};

double elapsed_ms(std::chrono::steady_clock::time_point start,
                  std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - start).count();
}

void download_hls_segments(const nlohmann::json& video, const fs::path& out_dir,
                           const std::string& request_text, const std::string& download_user_agent,
                           int cdn_strategy, int workers, long timeout, int retries,
                           int getplay_sleep_sec, int remain_seconds, bool add_segment_query,
                           bool simulate_player_buffer, double max_buffer_length,
                           double download_speed,
                           bool trace_enabled, bool sequential_playlist) {
  fs::create_directories(out_dir / "segments");
  std::string m3u8 = video.at("ul").value("m3u8", std::string());
  std::string base_url;
  if (video["ul"].contains("ui") && video["ul"]["ui"].is_array() && !video["ul"]["ui"].empty())
    base_url = video["ul"]["ui"][0].value("url", std::string());
  std::string remote_playlist_url = first_hls_playlist_url_from_video(video);
  auto headers = request_headers_for_segments(request_text, download_user_agent);
  int video_feature = video.value("feature", 0);
  int emergency_time_ms = -1;
  if (remain_seconds >= 0)
    emergency_time_ms = emergency_time_from_remain_seconds(remain_seconds);
  const bool lazy_emergency_query = simulate_player_buffer && add_segment_query;
  std::vector<std::string> embedded_uris = segment_uris(m3u8);
  if (!embedded_uris.empty()) {
    log_out() << "playlist=embedded segments=" << embedded_uris.size() << "\n";
  } else if (!remote_playlist_url.empty() || base_url.find(".m3u8") != std::string::npos) {
    try {
      if (remote_playlist_url.empty()) remote_playlist_url = base_url;
      const std::string playlist_url =
          normalize_segment_url(remote_playlist_url, video_feature, cdn_strategy,
                                emergency_time_ms);
      log_out() << "playlist remote fetch timeout=" << timeout
                << "s url=" << abbreviate_url_for_error(playlist_url) << "\n";
      Bytes remote = http_get_bytes(playlist_url, headers, timeout);
      std::string remote_m3u8(remote.begin(), remote.end());
      if (remote_m3u8.find("#EXTM3U") != std::string::npos &&
          !segment_uris(remote_m3u8).empty()) {
        m3u8 = std::move(remote_m3u8);
        log_out() << "playlist=remote\n";
      }
    } catch (const std::exception& e) {
      log_err() << "playlist remote failed, use embedded: " << e.what() << "\n";
    }
  }
  std::vector<std::pair<std::string, std::string>> segment_headers;
  segment_headers.reserve(headers.size());
  for (const auto& header : headers) {
    std::string key = header.first;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (key == "cookie") continue;
    segment_headers.push_back(header);
  }
  auto segments = build_segments(m3u8, base_url, video_feature, cdn_strategy,
                                 emergency_time_ms,
                                 add_segment_query && !lazy_emergency_query);
  log_out() << "segment_query video_feature=" << video_feature
            << " cdn_strategy=" << cdn_strategy
            << " dlipscene=" << ((video_feature & 4) != 0 ? 10 : 0)
            << " emergency_time_ms="
            << (lazy_emergency_query ? "simulated"
                                      : (emergency_time_ms >= 0
                                             ? std::to_string(emergency_time_ms)
                                             : "unknown"))
            << " mode=" << (add_segment_query ? "native" : "playlist") << "\n";
  if (simulate_player_buffer) {
    log_out() << "simulate_player_buffer=1 max_buffer_length="
              << max_buffer_length << "s download_speed=" << download_speed
              << "x\n";
    log_out() << "simulate_player_buffer: --workers disabled; using sequential playlist order\n";
  }
  auto local_material = local_material_from_video(video);
  if (local_material)
    log_out() << "local_transform=available keyid=" << local_material->keyid << "\n";
  write_text(out_dir / "source.m3u8", m3u8);
  write_local_playlist(m3u8, segments, out_dir / "local.m3u8");

  std::atomic<int> done{0}, cached{0}, downloaded{0}, transformed{0};
  std::atomic<uint64_t> downloaded_bytes{0};
  std::mutex trace_mu;
  DownloadProgress progress(done, downloaded_bytes, segments.size());
  auto download_one = [&](const Segment& seg, int retry_delay_seconds) {
    try {
    auto segment_start = std::chrono::steady_clock::now();
    fs::path dest = out_dir / "segments" / seg.filename;
    if (file_size_or_zero(dest) > 0) {
      auto read_start = std::chrono::steady_clock::now();
      Bytes data = read_bytes(dest);
      auto read_end = std::chrono::steady_clock::now();
      auto transform_start = std::chrono::steady_clock::now();
      bool did_transform = normalize_local_transport(data, local_material);
      auto transform_end = std::chrono::steady_clock::now();
      double write_ms = 0.0;
      if (did_transform) {
        auto write_start = std::chrono::steady_clock::now();
        write_bytes(dest, data);
        auto write_end = std::chrono::steady_clock::now();
        write_ms = elapsed_ms(write_start, write_end);
        transformed++;
      }
      if (trace_enabled) {
        std::lock_guard<std::mutex> lock(trace_mu);
        log_err() << "\nsegment_trace index=" << seg.index
                  << " cached=1 bytes=" << data.size()
                  << " read_ms=" << elapsed_ms(read_start, read_end)
                  << " transform_ms=" << elapsed_ms(transform_start, transform_end)
                  << " write_ms=" << write_ms
                  << " total_ms=" << elapsed_ms(segment_start, std::chrono::steady_clock::now())
                  << " transformed=" << (did_transform ? 1 : 0) << "\n";
      }
      cached++;
    } else {
      fs::path part = dest;
      part += ".part";
      std::string last_error;
      for (int attempt = 0; attempt <= retries; ++attempt) {
        try {
          auto curl_start = std::chrono::steady_clock::now();
          Bytes data = http_get_bytes(seg.url, segment_headers, timeout, &downloaded_bytes);
          auto curl_end = std::chrono::steady_clock::now();
          auto transform_start = std::chrono::steady_clock::now();
          bool did_transform = normalize_local_transport(data, local_material);
          auto transform_end = std::chrono::steady_clock::now();
          if (did_transform) transformed++;
          auto write_start = std::chrono::steady_clock::now();
          write_bytes(part, data);
          fs::rename(part, dest);
          auto write_end = std::chrono::steady_clock::now();
          if (trace_enabled) {
            std::lock_guard<std::mutex> lock(trace_mu);
            log_err() << "\nsegment_trace index=" << seg.index
                      << " cached=0 attempt=" << (attempt + 1)
                      << " bytes=" << data.size()
                      << " curl_ms=" << elapsed_ms(curl_start, curl_end)
                      << " transform_ms=" << elapsed_ms(transform_start, transform_end)
                      << " write_ms=" << elapsed_ms(write_start, write_end)
                      << " total_ms="
                      << elapsed_ms(segment_start, std::chrono::steady_clock::now())
                      << " transformed=" << (did_transform ? 1 : 0) << "\n";
          }
          downloaded++;
          last_error.clear();
          break;
        } catch (const std::exception& e) {
          std::ostringstream err;
          err << "segment " << seg.index << " (" << seg.filename << ") attempt "
              << (attempt + 1) << "/" << (retries + 1) << " failed: "
              << e.what() << " url=" << abbreviate_url_for_error(seg.url);
          last_error = err.str();
          log_err() << last_error << "\n";
          if (attempt < retries) {
            if (retry_delay_seconds > 0)
              std::this_thread::sleep_for(std::chrono::seconds(retry_delay_seconds));
            else
              std::this_thread::sleep_for(
                  std::chrono::milliseconds(250 * (attempt + 1)));
          }
        }
      }
      if (!last_error.empty()) fail(last_error);
    }
    int n = ++done;
    if (n == 1 || n == static_cast<int>(segments.size()) || n % 10 == 0)
      log_out() << "download progress " << n << "/" << segments.size() << "\n";
    } catch (const std::exception& e) {
      std::string msg = e.what();
      if (msg.rfind("segment ", 0) == 0) fail(msg);
      fail("segment " + std::to_string(seg.index) + " (" + seg.filename +
           ") failed: " + msg + " url=" + abbreviate_url_for_error(seg.url));
    }
  };

  auto is_getplay_segment = [](const Segment& seg) {
    return seg.url.find("/getPlayTs/") != std::string::npos ||
           seg.uri.find("/getPlayTs/") != std::string::npos;
  };

  std::vector<size_t> getplay_indices;
  for (size_t i = 0; i < segments.size(); ++i) {
    if (is_getplay_segment(segments[i])) getplay_indices.push_back(i);
  }

  if (simulate_player_buffer) {
    log_out() << "segments simulated_player_buffer=" << segments.size()
              << " mode=playlist-order\n";
    getplay_sleep_sec = std::max(0, getplay_sleep_sec);
    double buffer_seconds = 0.0;
    bool previous_getplay_download = false;
    auto consume_playback = [&](double elapsed_seconds) {
      if (elapsed_seconds <= 0.0) return;
      buffer_seconds = std::max(0.0, buffer_seconds - elapsed_seconds * download_speed);
    };
    auto maybe_wait_for_buffer_cap = [&]() {
      if (max_buffer_length <= 0.0 || buffer_seconds <= max_buffer_length) return;
      double wait_seconds = (buffer_seconds - max_buffer_length) / download_speed;
      if (wait_seconds <= 0.0) return;
      if (trace_enabled)
        log_err() << "\nbuffer_trace wait_s=" << wait_seconds
                  << " before=" << buffer_seconds
                  << " cap=" << max_buffer_length << "\n";
      std::this_thread::sleep_for(std::chrono::duration<double>(wait_seconds));
      consume_playback(wait_seconds);
    };
    auto segment_for_current_buffer = [&](const Segment& seg) {
      Segment request_seg = seg;
      if (add_segment_query) {
        int simulated_remain_seconds = static_cast<int>(buffer_seconds);
        int simulated_emergency_time =
            emergency_time_from_remain_seconds(simulated_remain_seconds);
        request_seg.url = normalize_segment_url(
            url_join_simple(base_url, seg.uri), video_feature, cdn_strategy,
            simulated_emergency_time);
        if (trace_enabled)
          log_err() << "\nbuffer_trace index=" << seg.index
                    << " buffer_s=" << buffer_seconds
                    << " remain_seconds=" << simulated_remain_seconds
                    << " emergency_time_ms=" << simulated_emergency_time
                    << " emergency_level="
                    << (simulated_emergency_time <= 1000 ? 1 : 0) << "\n";
      }
      return request_seg;
    };

    for (const auto& seg : segments) {
      bool is_getplay = is_getplay_segment(seg);
      bool needs_download =
          file_size_or_zero(out_dir / "segments" / seg.filename) == 0;
      if (is_getplay && needs_download && previous_getplay_download &&
          getplay_sleep_sec > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(getplay_sleep_sec));
        consume_playback(static_cast<double>(getplay_sleep_sec));
      }
      if (is_getplay)
        log_out() << "getPlayTs segment index=" << seg.index << "\n";
      Segment request_seg = segment_for_current_buffer(seg);
      auto request_start = std::chrono::steady_clock::now();
      download_one(request_seg, is_getplay ? 10 : 0);
      auto request_end = std::chrono::steady_clock::now();
      consume_playback(elapsed_ms(request_start, request_end) / 1000.0);
      if (!seg.init_map) buffer_seconds += std::max(0.0, seg.duration_seconds);
      if (trace_enabled)
        log_err() << "\nbuffer_trace index=" << seg.index
                  << " duration_s=" << seg.duration_seconds
                  << " after_buffer_s=" << buffer_seconds << "\n";
      maybe_wait_for_buffer_cap();
      previous_getplay_download = is_getplay && needs_download;
    }
  } else if (sequential_playlist) {
    log_out() << "segments sequential=" << segments.size()
              << " mode=playlist-order\n";
    getplay_sleep_sec = std::max(0, getplay_sleep_sec);
    bool previous_getplay_download = false;
    for (const auto& seg : segments) {
      bool is_getplay = is_getplay_segment(seg);
      bool needs_download =
          file_size_or_zero(out_dir / "segments" / seg.filename) == 0;
      if (is_getplay && needs_download && previous_getplay_download &&
          getplay_sleep_sec > 0)
        std::this_thread::sleep_for(std::chrono::seconds(getplay_sleep_sec));
      if (is_getplay)
        log_out() << "getPlayTs segment index=" << seg.index << "\n";
      download_one(seg, is_getplay ? 10 : 0);
      previous_getplay_download = is_getplay && needs_download;
    }
  } else {
    getplay_sleep_sec = std::max(0, getplay_sleep_sec);
    size_t cursor = 0;
    size_t getplay_ordinal = 0;
    while (cursor < segments.size()) {
      if (is_getplay_segment(segments[cursor])) {
        log_out() << "getPlayTs sequential=" << getplay_indices.size()
                  << " sleep=" << getplay_sleep_sec << "s\n";
        bool previous_getplay_download = false;
        while (cursor < segments.size() && is_getplay_segment(segments[cursor])) {
          const Segment& seg = segments[cursor];
          bool needs_download =
              file_size_or_zero(out_dir / "segments" / seg.filename) == 0;
          if (needs_download && previous_getplay_download && getplay_sleep_sec > 0)
            std::this_thread::sleep_for(std::chrono::seconds(getplay_sleep_sec));
          ++getplay_ordinal;
          log_out() << "getPlayTs segment " << getplay_ordinal << "/"
                    << getplay_indices.size() << " index=" << seg.index << "\n";
          download_one(seg, 10);
          if (needs_download) previous_getplay_download = true;
          ++cursor;
        }
        continue;
      }

      size_t run_start = cursor;
      while (cursor < segments.size() && !is_getplay_segment(segments[cursor])) {
        ++cursor;
      }
      size_t run_count = cursor - run_start;
      log_out() << "segments ordered_parallel=" << run_count
                << " workers=" << std::max(1, workers)
                << " start_index=" << segments[run_start].index
                << " end_index=" << segments[cursor - 1].index << "\n";
      parallel_for(run_count, workers, [&](size_t n) {
        download_one(segments[run_start + n], 0);
      });
    }
  }
  progress.stop();

  std::ofstream merged(out_dir / "merged.ts", std::ios::binary);
  uint64_t total = 0;
  for (const auto& seg : segments) {
    Bytes data = read_bytes(out_dir / "segments" / seg.filename);
    merged.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    total += data.size();
  }
  nlohmann::json meta{{"vid", video.value("vid", "")},
                      {"fn", video.value("fn", "")},
                      {"segment_count", segments.size()},
                      {"cached", cached.load()},
                      {"downloaded", downloaded.load()},
                      {"local_transformed", transformed.load()},
                      {"getplay_sequential", getplay_indices.size()},
                      {"cdn_strategy", cdn_strategy},
                      {"merged_bytes", total}};
  write_text(out_dir / "metadata.json", meta.dump(2) + "\n");
  log_out() << "downloaded=" << downloaded << " cached=" << cached
            << " local_transformed=" << transformed
            << " merged_bytes=" << total << "\n";
}

class TxsddChacha20V2 {
 public:
  TxsddChacha20V2(const std::string& key, const std::string& nonce) {
    if (key.size() != 32 || nonce.size() != 12) fail("Txsdd key=32 nonce=12 ASCII");
    Bytes kb = bytes_from_ascii(key);
    Bytes nb = bytes_from_ascii(nonce);
    for (int i = 0; i < 8; ++i) key_words_[i] = le32(&kb[i * 4]);
    for (int i = 0; i < 3; ++i) nonce_words_[i] = le32(&nb[i * 4]);
  }

  static void quarter_round(std::array<uint32_t, 16>& s, int a, int b, int c, int d) {
    s[a] -= s[b]; s[d] ^= s[a]; s[d] = rotl32(s[d], 16);
    s[c] += s[d]; s[b] ^= s[c]; s[b] = rotl32(s[b], 12);
    s[a] += s[b]; s[d] ^= s[a]; s[d] = rotl32(s[d], 8);
    s[c] -= s[d]; s[b] ^= s[c]; s[b] = rotl32(s[b], 6);
  }

  Bytes block(uint64_t counter64) const {
    uint32_t lo = static_cast<uint32_t>(counter64);
    uint32_t hi = static_cast<uint32_t>(counter64 >> 32);
    const auto& k = key_words_;
    const auto& n = nonce_words_;
    std::array<uint32_t, 16> state{0x63713531u, 0x78953552u, 0x86392848u, k[1], k[3], k[4],
                                   lo,          k[7],        k[2],        k[0], k[6], n[0],
                                   n[1],        n[2],        hi,          k[5]};
    auto w = state;
    for (int i = 0; i < 10; ++i) {
      quarter_round(w, 0, 4, 8, 12); quarter_round(w, 1, 5, 9, 13);
      quarter_round(w, 2, 6, 10, 14); quarter_round(w, 3, 7, 11, 15);
      quarter_round(w, 0, 5, 10, 15); quarter_round(w, 1, 6, 11, 12);
      quarter_round(w, 2, 7, 8, 13); quarter_round(w, 3, 4, 9, 14);
    }
    Bytes out;
    out.reserve(64);
    for (int i = 0; i < 16; ++i) put_le32(out, w[i] + state[i]);
    return out;
  }

  Bytes keystream_slice(uint64_t offset, size_t length) {
    uint64_t cache_key = (offset << 8) ^ length;
    auto it = cache_.find(cache_key);
    if (it != cache_.end()) return it->second;
    Bytes out(length);
    uint64_t counter = offset >> 6;
    size_t skip = offset & 0x3f;
    size_t pos = 0;
    while (pos < length) {
      Bytes b = block(counter++);
      size_t take = std::min<size_t>(length - pos, 64 - skip);
      std::copy(b.begin() + static_cast<std::ptrdiff_t>(skip),
                b.begin() + static_cast<std::ptrdiff_t>(skip + take), out.begin() + pos);
      pos += take;
      skip = 0;
    }
    cache_[cache_key] = out;
    return out;
  }

  void xor_range(Bytes& buf, size_t start, size_t length, uint64_t offset) {
    if (length == 0) return;
    Bytes ks = keystream_slice(offset, length);
    for (size_t i = 0; i < length; ++i) buf[start + i] ^= ks[i];
  }

 private:
  std::array<uint32_t, 8> key_words_{};
  std::array<uint32_t, 3> nonce_words_{};
  std::unordered_map<uint64_t, Bytes> cache_;
};

struct DecryptStats {
  int packets_total = 0;
  int pes_units = 0;
  int nals_seen = 0;
  int selected_nals = 0;
  int transformed_nals = 0;
  int transformed_chunks = 0;
  int transformed_bytes = 0;
  int video_pid = -1;
  std::string video_codec;
};

// Native DRM NAL selection started as a C++ port of
// ../analytics/app/decrypt_tvk_video.v1.py (HEVC path). Tencent's txsdd transform keeps
// codec headers clear and applies the same 48/64/192 byte pattern inside VCL
// NALs, so the CLI only needs codec-specific PMT IDs and NAL type extraction
// for AVC/HEVC/VVC.
enum class VideoCodec {
  kUnknown,
  kAvc,
  kHevc,
  kVvc,
};

struct VideoStream {
  int pid = -1;
  VideoCodec codec = VideoCodec::kUnknown;
};

const char* codec_name(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kAvc: return "h264";
    case VideoCodec::kHevc: return "h265";
    case VideoCodec::kVvc: return "h266";
    default: return "unknown";
  }
}

const char* codec_label(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kAvc: return "H.264/AVC";
    case VideoCodec::kHevc: return "H.265/HEVC";
    case VideoCodec::kVvc: return "H.266/VVC";
    default: return "unknown";
  }
}

int nal_type_for_codec(const Bytes& nal, VideoCodec codec) {
  if (nal.empty()) return -1;
  switch (codec) {
    case VideoCodec::kAvc:
      return nal[0] & 0x1f;
    case VideoCodec::kHevc:
      return nal.size() < 2 ? -1 : ((nal[0] >> 1) & 0x3f);
    case VideoCodec::kVvc:
      return nal.size() < 2 ? -1 : ((nal[1] >> 3) & 0x1f);
    default:
      return -1;
  }
}

bool nal_header_valid_for_codec(const Bytes& nal, VideoCodec codec) {
  if (nal.empty()) return false;
  switch (codec) {
    case VideoCodec::kAvc:
      return (nal[0] & 0x80) == 0;
    case VideoCodec::kHevc:
      return nal.size() >= 2 && (nal[1] & 0x07) != 0;
    case VideoCodec::kVvc:
      return nal.size() >= 2 && (nal[0] & 0xc0) == 0 && (nal[1] & 0x07) != 0;
    default:
      return false;
  }
}

bool is_vcl_nal(VideoCodec codec, int nal_type) {
  switch (codec) {
    case VideoCodec::kAvc:
      return nal_type >= 1 && nal_type <= 5;
    case VideoCodec::kHevc:
      return (nal_type >= 0 && nal_type <= 9) || (nal_type >= 16 && nal_type <= 21);
    case VideoCodec::kVvc:
      return nal_type >= 0 && nal_type <= 11;
    default:
      return false;
  }
}

bool is_parameter_nal(VideoCodec codec, int nal_type) {
  switch (codec) {
    case VideoCodec::kAvc:
      return nal_type == 7 || nal_type == 8;
    case VideoCodec::kHevc:
      return nal_type == 32 || nal_type == 33 || nal_type == 34 || nal_type == 35 ||
             nal_type == 39 || nal_type == 40;
    case VideoCodec::kVvc:
      return (nal_type >= 12 && nal_type <= 17) || nal_type == 20 ||
             nal_type == 23 || nal_type == 24;
    default:
      return false;
  }
}

std::pair<int, int> find_start_code(const Bytes& buf, size_t start) {
  if (buf.size() < 3) return {-1, -1};
  size_t limit = buf.size() - 3;
  for (size_t i = start; i < limit; ++i) {
    if (buf[i] == 0 && buf[i + 1] == 0) {
      if (buf[i + 2] == 1) return {static_cast<int>(i), 3};
      if (i + 3 < buf.size() && buf[i + 2] == 0 && buf[i + 3] == 1)
        return {static_cast<int>(i), 4};
    }
  }
  return {-1, -1};
}

std::vector<std::tuple<size_t, size_t, int>> iter_annexb_nals(const Bytes& buf) {
  std::vector<std::tuple<size_t, size_t, int>> out;
  size_t cursor = 0;
  while (true) {
    auto [start_i, prefix_len] = find_start_code(buf, cursor);
    if (start_i < 0) break;
    size_t header_start = static_cast<size_t>(start_i + prefix_len);
    auto [next_i, ignored] = find_start_code(buf, header_start);
    (void)ignored;
    size_t end = next_i < 0 ? buf.size() : static_cast<size_t>(next_i);
    if (header_start < end) out.emplace_back(header_start, end, prefix_len);
    cursor = end;
  }
  return out;
}

Bytes remove_emulation_prevention(const Bytes& nal) {
  Bytes out;
  out.reserve(nal.size());
  size_t i = 0;
  while (i < nal.size()) {
    if (i + 3 < nal.size() && nal[i] == 0 && nal[i + 1] == 0 && nal[i + 2] == 3 &&
        nal[i + 3] <= 3) {
      out.push_back(nal[i]);
      out.push_back(nal[i + 1]);
      i += 3;
      continue;
    }
    out.push_back(nal[i++]);
  }
  return out;
}

Bytes decrypt_selected_video_nal(const Bytes& nal, VideoCodec codec, TxsddChacha20V2& cipher,
                                 DecryptStats& stats) {
  int nal_type = nal_type_for_codec(nal, codec);
  if (nal_type < 0) return nal;
  stats.nals_seen++;
  if (!is_vcl_nal(codec, nal_type)) return nal;
  stats.selected_nals++;
  Bytes normalized = remove_emulation_prevention(nal);
  if (normalized.size() <= kInitialClear) return normalized;
  size_t chunk_start = kInitialClear;
  uint64_t local_offset = kInitialClear;
  bool touched = false;
  while (chunk_start + kProtectedLen <= normalized.size()) {
    cipher.xor_range(normalized, chunk_start, kProtectedLen, local_offset);
    stats.transformed_chunks++;
    stats.transformed_bytes += kProtectedLen;
    touched = true;
    chunk_start += kStride;
    local_offset += kStride;
  }
  if (touched) stats.transformed_nals++;
  return normalized;
}

void decrypt_selected_video_nal_in_place(Bytes& buf, size_t start, size_t length,
                                         VideoCodec codec, TxsddChacha20V2& cipher,
                                         DecryptStats& stats) {
  if (start >= buf.size() || length == 0 || start + length > buf.size()) return;
  size_t header_len = std::min<size_t>(length, 2);
  Bytes header(buf.begin() + static_cast<std::ptrdiff_t>(start),
               buf.begin() + static_cast<std::ptrdiff_t>(start + header_len));
  int nal_type = nal_type_for_codec(header, codec);
  if (nal_type < 0) return;
  stats.nals_seen++;
  if (!is_vcl_nal(codec, nal_type)) return;
  stats.selected_nals++;
  if (length <= kInitialClear) return;
  size_t nal_end = start + length;
  size_t chunk_start = start + kInitialClear;
  uint64_t local_offset = kInitialClear;
  bool touched = false;
  while (chunk_start + kProtectedLen <= nal_end) {
    cipher.xor_range(buf, chunk_start, kProtectedLen, local_offset);
    stats.transformed_bytes += kProtectedLen;
    chunk_start += kStride;
    local_offset += kStride;
    touched = true;
  }
  if (touched) stats.transformed_nals++;
}

void decrypt_annexb_in_place(Bytes& buf, VideoCodec codec, TxsddChacha20V2& cipher,
                             DecryptStats& stats) {
  Bytes result;
  size_t cursor = 0;
  for (const auto& item : iter_annexb_nals(buf)) {
    auto [header_start, end, prefix_len] = item;
    size_t prefix_start = header_start - static_cast<size_t>(prefix_len);
    result.insert(result.end(), buf.begin() + static_cast<std::ptrdiff_t>(cursor),
                  buf.begin() + static_cast<std::ptrdiff_t>(prefix_start));
    result.insert(result.end(), buf.begin() + static_cast<std::ptrdiff_t>(prefix_start),
                  buf.begin() + static_cast<std::ptrdiff_t>(header_start));
    Bytes nal(buf.begin() + static_cast<std::ptrdiff_t>(header_start),
              buf.begin() + static_cast<std::ptrdiff_t>(end));
    Bytes repl = decrypt_selected_video_nal(nal, codec, cipher, stats);
    result.insert(result.end(), repl.begin(), repl.end());
    cursor = end;
  }
  result.insert(result.end(), buf.begin() + static_cast<std::ptrdiff_t>(cursor), buf.end());
  buf.swap(result);
}

size_t pes_payload_offset(const Bytes& pes) {
  if (pes.size() < 6 || pes[0] != 0 || pes[1] != 0 || pes[2] != 1) return 0;
  if (pes.size() < 9) return 6;
  return std::min<size_t>(pes.size(), 9 + pes[8]);
}

void write_back_slices(Bytes& buf, const std::vector<std::pair<size_t, size_t>>& spans,
                       const Bytes& payload) {
  size_t total = 0;
  for (auto [s, e] : spans) total += e - s;
  if (payload.size() > total) fail("PES writeback payload is larger than the target spans");
  size_t cursor = 0;
  size_t remaining = payload.size();
  for (auto [start, end] : spans) {
    size_t span_len = end - start;
    size_t take = std::min(span_len, remaining);
    std::copy(payload.begin() + static_cast<std::ptrdiff_t>(cursor),
              payload.begin() + static_cast<std::ptrdiff_t>(cursor + take),
              buf.begin() + static_cast<std::ptrdiff_t>(start));
    if (take < span_len)
      std::fill(buf.begin() + static_cast<std::ptrdiff_t>(start + take),
                buf.begin() + static_cast<std::ptrdiff_t>(end), 0);
    cursor += take;
    remaining -= take;
  }
}

std::vector<Bytes> feed_psi_section(std::map<int, Bytes>& buffers,
                                    std::map<int, int>& expected_lengths, int pid,
                                    const Bytes& payload, bool payload_unit_start) {
  std::vector<Bytes> completed;
  Bytes& buf = buffers[pid];
  int expected = expected_lengths.count(pid) ? expected_lengths[pid] : -1;
  size_t pos = 0;
  if (payload_unit_start) {
    if (payload.empty()) return completed;
    int pointer = payload[0];
    pos = 1;
    if (pointer) {
      size_t carry_end = std::min(payload.size(), pos + static_cast<size_t>(pointer));
      buf.insert(buf.end(), payload.begin() + static_cast<std::ptrdiff_t>(pos),
                 payload.begin() + static_cast<std::ptrdiff_t>(carry_end));
      if (expected < 0 && buf.size() >= 3) expected = 3 + (((buf[1] & 0x0f) << 8) | buf[2]);
      if (expected >= 0 && buf.size() >= static_cast<size_t>(expected))
        completed.emplace_back(buf.begin(), buf.begin() + expected);
      buf.clear();
      expected = -1;
    }
    pos += static_cast<size_t>(pointer);
  }
  if (pos < payload.size())
    buf.insert(buf.end(), payload.begin() + static_cast<std::ptrdiff_t>(pos), payload.end());
  while (true) {
    if (expected < 0 && buf.size() >= 3) expected = 3 + (((buf[1] & 0x0f) << 8) | buf[2]);
    if (expected < 0 || buf.size() < static_cast<size_t>(expected)) break;
    completed.emplace_back(buf.begin(), buf.begin() + expected);
    buf.erase(buf.begin(), buf.begin() + expected);
    expected = -1;
  }
  expected_lengths[pid] = expected;
  return completed;
}

std::vector<int> parse_pat_section(const Bytes& section) {
  if (section.size() < 12 || section[0] != 0) return {};
  int section_length = ((section[1] & 0x0f) << 8) | section[2];
  int end = 3 + section_length - 4;
  std::vector<int> pids;
  for (int off = 8; off + 4 <= end && off + 4 <= static_cast<int>(section.size()); off += 4) {
    int program_number = (section[off] << 8) | section[off + 1];
    int program_pid = ((section[off + 2] & 0x1f) << 8) | section[off + 3];
    if (program_number != 0) pids.push_back(program_pid);
  }
  return pids;
}

VideoStream parse_pmt_section(const Bytes& section) {
  if (section.size() < 16 || section[0] != 0x02) return {};
  int section_length = ((section[1] & 0x0f) << 8) | section[2];
  int program_info_length = ((section[10] & 0x0f) << 8) | section[11];
  int offset = 12 + program_info_length;
  int end = 3 + section_length - 4;
  VideoStream avc, hevc, vvc;
  while (offset + 5 <= end && offset + 5 <= static_cast<int>(section.size())) {
    int stream_type = section[offset];
    int elementary_pid = ((section[offset + 1] & 0x1f) << 8) | section[offset + 2];
    int es_info_length = ((section[offset + 3] & 0x0f) << 8) | section[offset + 4];
    if (stream_type == 0x1b && avc.pid < 0) avc = {elementary_pid, VideoCodec::kAvc};
    else if (stream_type == 0x24 && hevc.pid < 0) hevc = {elementary_pid, VideoCodec::kHevc};
    else if (stream_type == 0x33 && vvc.pid < 0) vvc = {elementary_pid, VideoCodec::kVvc};
    offset += 5 + es_info_length;
  }
  if (vvc.pid >= 0) return vvc;
  if (hevc.pid >= 0) return hevc;
  return avc;
}

int annexb_codec_score(const Bytes& payload, VideoCodec codec) {
  int found = 0, parameter_sets = 0, vcl = 0;
  for (const auto& item : iter_annexb_nals(payload)) {
    auto [header_start, end, prefix_len] = item;
    (void)prefix_len;
    Bytes nal(payload.begin() + static_cast<std::ptrdiff_t>(header_start),
              payload.begin() + static_cast<std::ptrdiff_t>(end));
    if (!nal_header_valid_for_codec(nal, codec)) continue;
    int nal_type = nal_type_for_codec(nal, codec);
    if (nal_type < 0) continue;
    found++;
    if (is_parameter_nal(codec, nal_type)) parameter_sets++;
    if (is_vcl_nal(codec, nal_type)) vcl++;
    if (found >= 8) break;
  }
  if (found == 0 || (parameter_sets == 0 && vcl == 0)) return 0;
  return found + parameter_sets * 30 + vcl * 10;
}

VideoCodec detect_annexb_codec(const Bytes& payload) {
  VideoCodec best = VideoCodec::kUnknown;
  int best_score = 0;
  for (VideoCodec codec : {VideoCodec::kAvc, VideoCodec::kHevc, VideoCodec::kVvc}) {
    int score = annexb_codec_score(payload, codec);
    if (score > best_score) {
      best_score = score;
      best = codec;
    }
  }
  return best;
}

VideoStream sniff_video_pid_from_ts(const Bytes& buf) {
  int assembling_pid = -1;
  std::vector<Bytes> spans;
  std::map<int, Bytes> candidates;
  auto flush = [&] {
    if (assembling_pid < 0 || spans.empty() || candidates.count(assembling_pid)) {
      assembling_pid = -1;
      spans.clear();
      return;
    }
    Bytes joined;
    for (const auto& s : spans) joined.insert(joined.end(), s.begin(), s.end());
    candidates[assembling_pid] = joined;
    assembling_pid = -1;
    spans.clear();
  };
  for (size_t off = 0; off + kTsPacketSize <= buf.size(); off += kTsPacketSize) {
    if (buf[off] != kTsSync) continue;
    uint8_t b1 = buf[off + 1], b2 = buf[off + 2], b3 = buf[off + 3];
    bool start = b1 & 0x40;
    int pid = ((b1 & 0x1f) << 8) | b2;
    int afc = (b3 >> 4) & 3;
    bool has_payload = afc == 1 || afc == 3;
    bool has_adaptation = afc == 2 || afc == 3;
    if (!has_payload) continue;
    size_t payload_start = off + 4;
    if (has_adaptation) payload_start += 1 + buf[payload_start];
    size_t payload_end = off + kTsPacketSize;
    if (payload_start >= payload_end) continue;
    Bytes payload(buf.begin() + static_cast<std::ptrdiff_t>(payload_start),
                  buf.begin() + static_cast<std::ptrdiff_t>(payload_end));
    if (start) {
      flush();
      if (payload.size() >= 3 && payload[0] == 0 && payload[1] == 0 && payload[2] == 1) {
        assembling_pid = pid;
        spans = {payload};
      }
    } else if (assembling_pid == pid) {
      spans.push_back(payload);
      size_t total = 0;
      for (const auto& s : spans) total += s.size();
      if (total >= 65536) flush();
    }
  }
  flush();
  VideoStream best;
  int best_score = 0;
  size_t best_size = 0;
  for (const auto& kv : candidates) {
    Bytes pes = kv.second;
    size_t payload_off = pes_payload_offset(pes);
    Bytes payload(pes.begin() + static_cast<std::ptrdiff_t>(payload_off), pes.end());
    VideoCodec codec = detect_annexb_codec(payload);
    int score = annexb_codec_score(payload, codec);
    if (score > best_score || (score == best_score && score > 0 && pes.size() > best_size)) {
      best = {kv.first, codec};
      best_score = score;
      best_size = pes.size();
    }
  }
  return best;
}

VideoStream discover_video_stream_ts(const Bytes& buf, bool trace_enabled) {
  std::map<int, Bytes> psi_buffers;
  std::map<int, int> psi_expected;
  std::vector<int> pmt_pids;
  for (size_t off = 0; off + kTsPacketSize <= buf.size(); off += kTsPacketSize) {
    if (buf[off] != kTsSync) fail("missing TS sync byte at offset " + std::to_string(off));
    uint8_t b1 = buf[off + 1], b2 = buf[off + 2], b3 = buf[off + 3];
    bool start = b1 & 0x40;
    int pid = ((b1 & 0x1f) << 8) | b2;
    int afc = (b3 >> 4) & 3;
    bool has_payload = afc == 1 || afc == 3;
    bool has_adaptation = afc == 2 || afc == 3;
    size_t payload_start = off + 4;
    if (has_adaptation) payload_start += 1 + buf[payload_start];
    size_t payload_end = off + kTsPacketSize;
    if (!has_payload || payload_start >= payload_end) continue;
    if (pid != 0 && std::find(pmt_pids.begin(), pmt_pids.end(), pid) == pmt_pids.end())
      continue;
    Bytes payload(buf.begin() + static_cast<std::ptrdiff_t>(payload_start),
                  buf.begin() + static_cast<std::ptrdiff_t>(payload_end));
    for (const Bytes& section : feed_psi_section(psi_buffers, psi_expected, pid, payload, start)) {
      if (pid == 0) {
        auto pids = parse_pat_section(section);
        pmt_pids.insert(pmt_pids.end(), pids.begin(), pids.end());
      } else {
        VideoStream stream = parse_pmt_section(section);
        if (stream.pid >= 0) {
          trace(trace_enabled, std::string("[ts] ") + codec_label(stream.codec) +
                                   " video PID=0x" +
                                   util::hex_encode(Bytes{static_cast<uint8_t>(stream.pid >> 8),
                                                     static_cast<uint8_t>(stream.pid)}));
          return stream;
        }
      }
    }
  }
  VideoStream sniffed = sniff_video_pid_from_ts(buf);
  if (sniffed.pid >= 0) return sniffed;
  fail("Could not discover an H.264/H.265/H.266 video PID");
}

DecryptStats decrypt_ts_in_place(Bytes& buf, TxsddChacha20V2& cipher, bool trace_enabled) {
  if (buf.size() % kTsPacketSize != 0) fail("TS length is not a multiple of 188");
  DecryptStats stats;
  VideoStream video = discover_video_stream_ts(buf, trace_enabled);
  stats.video_pid = video.pid;
  stats.video_codec = codec_name(video.codec);
  std::vector<std::pair<size_t, size_t>> pes_spans;
  auto finalize = [&] {
    if (pes_spans.empty()) return;
    Bytes pes;
    for (auto [s, e] : pes_spans)
      pes.insert(pes.end(), buf.begin() + static_cast<std::ptrdiff_t>(s),
                 buf.begin() + static_cast<std::ptrdiff_t>(e));
    size_t payload_off = pes_payload_offset(pes);
    Bytes payload(pes.begin() + static_cast<std::ptrdiff_t>(payload_off), pes.end());
    decrypt_annexb_in_place(payload, video.codec, cipher, stats);
    pes.erase(pes.begin() + static_cast<std::ptrdiff_t>(payload_off), pes.end());
    pes.insert(pes.end(), payload.begin(), payload.end());
    write_back_slices(buf, pes_spans, pes);
    stats.pes_units++;
    pes_spans.clear();
  };
  for (size_t off = 0; off + kTsPacketSize <= buf.size(); off += kTsPacketSize) {
    stats.packets_total++;
    if (buf[off] != kTsSync) fail("missing TS sync byte at offset " + std::to_string(off));
    uint8_t b1 = buf[off + 1], b2 = buf[off + 2], b3 = buf[off + 3];
    bool start = b1 & 0x40;
    int pid = ((b1 & 0x1f) << 8) | b2;
    int afc = (b3 >> 4) & 3;
    bool has_payload = afc == 1 || afc == 3;
    bool has_adaptation = afc == 2 || afc == 3;
    size_t payload_start = off + 4;
    if (has_adaptation) payload_start += 1 + buf[payload_start];
    size_t payload_end = off + kTsPacketSize;
    if (!has_payload || payload_start >= payload_end) continue;
    if (pid != video.pid) continue;
    if (start) finalize();
    pes_spans.emplace_back(payload_start, payload_end);
  }
  finalize();
  return stats;
}

bool is_ts(const Bytes& data) {
  if (data.size() < kTsPacketSize * 4) return false;
  for (size_t off = 0; off < kTsPacketSize * 4; off += kTsPacketSize)
    if (data[off] != kTsSync) return false;
  return true;
}

struct Mp4Box {
  size_t start = 0;
  size_t header = 0;
  size_t payload = 0;
  size_t end = 0;
  std::string type;
};

bool read_mp4_box(const Bytes& data, size_t offset, size_t limit, Mp4Box& box) {
  if (offset + 8 > limit || limit > data.size()) return false;
  uint64_t size = be32(&data[offset]);
  size_t header = 8;
  if (size == 1) {
    if (offset + 16 > limit) return false;
    size = (uint64_t(be32(&data[offset + 8])) << 32) | be32(&data[offset + 12]);
    header = 16;
  } else if (size == 0) {
    size = limit - offset;
  }
  if (size < header || offset + size > limit) return false;
  box = {offset, header, offset + header, offset + static_cast<size_t>(size),
         std::string(reinterpret_cast<const char*>(&data[offset + 4]), 4)};
  return true;
}

bool is_mp4_like(const Bytes& data) {
  if (data.size() < 8) return false;
  Mp4Box box;
  if (!read_mp4_box(data, 0, data.size(), box)) return false;
  return box.type == "ftyp" || box.type == "styp" || box.type == "moov" ||
         box.type == "sidx" || box.type == "moof" || box.type == "mdat";
}

struct Mp4InitInfo {
  int video_track_id = -1;
  VideoCodec codec = VideoCodec::kUnknown;
  int nal_length_size = 4;
};

struct TrackInfo {
  int track_id = -1;
  bool video = false;
  VideoCodec codec = VideoCodec::kUnknown;
  int nal_length_size = 4;
};

VideoCodec codec_from_sample_entry(const std::string& type) {
  if (type == "avc1" || type == "avc3") return VideoCodec::kAvc;
  if (type == "hvc1" || type == "hev1") return VideoCodec::kHevc;
  if (type == "vvc1" || type == "vvi1") return VideoCodec::kVvc;
  return VideoCodec::kUnknown;
}

int avcc_nal_length_size(const Bytes& data, size_t start, size_t end) {
  if (start + 5 <= end) return (data[start + 4] & 0x03) + 1;
  return 4;
}

int hvcc_nal_length_size(const Bytes& data, size_t start, size_t end) {
  if (start + 22 <= end) return (data[start + 21] & 0x03) + 1;
  return 4;
}

int vvcc_nal_length_size(const Bytes& data, size_t start, size_t end) {
  if (start + 5 <= end) return (data[start + 4] & 0x03) + 1;
  return 4;
}

void parse_sample_entry_children(const Bytes& data, size_t start, size_t end,
                                 TrackInfo& track) {
  size_t pos = start;
  while (pos + 8 <= end) {
    Mp4Box box;
    if (!read_mp4_box(data, pos, end, box)) break;
    if (box.type == "avcC")
      track.nal_length_size = avcc_nal_length_size(data, box.payload, box.end);
    else if (box.type == "hvcC")
      track.nal_length_size = hvcc_nal_length_size(data, box.payload, box.end);
    else if (box.type == "vvcC" || box.type == "vvcc")
      track.nal_length_size = vvcc_nal_length_size(data, box.payload, box.end);
    pos = box.end;
  }
}

void parse_stsd(const Bytes& data, const Mp4Box& stsd, TrackInfo& track) {
  if (stsd.payload + 8 > stsd.end) return;
  uint32_t entries = be32(&data[stsd.payload + 4]);
  size_t pos = stsd.payload + 8;
  for (uint32_t i = 0; i < entries && pos + 8 <= stsd.end; ++i) {
    Mp4Box entry;
    if (!read_mp4_box(data, pos, stsd.end, entry)) return;
    VideoCodec codec = codec_from_sample_entry(entry.type);
    if (codec != VideoCodec::kUnknown) {
      track.codec = codec;
      // VisualSampleEntry children start after the fixed 78-byte sample entry header.
      size_t child_start = std::min(entry.end, entry.start + static_cast<size_t>(86));
      parse_sample_entry_children(data, child_start, entry.end, track);
    }
    pos = entry.end;
  }
}

void parse_trak_box(const Bytes& data, const Mp4Box& trak, TrackInfo& track) {
  size_t pos = trak.payload;
  while (pos + 8 <= trak.end) {
    Mp4Box box;
    if (!read_mp4_box(data, pos, trak.end, box)) break;
    if (box.type == "tkhd" && box.payload + 24 <= box.end) {
      uint8_t version = data[box.payload];
      size_t track_id_pos = version == 1 ? box.payload + 20 : box.payload + 12;
      if (track_id_pos + 4 <= box.end)
        track.track_id = static_cast<int>(be32(&data[track_id_pos]));
    } else if (box.type == "mdia") {
      size_t mdia_pos = box.payload;
      while (mdia_pos + 8 <= box.end) {
        Mp4Box mdia_box;
        if (!read_mp4_box(data, mdia_pos, box.end, mdia_box)) break;
        if (mdia_box.type == "hdlr" && mdia_box.payload + 12 <= mdia_box.end) {
          std::string handler(reinterpret_cast<const char*>(&data[mdia_box.payload + 8]), 4);
          track.video = handler == "vide";
        } else if (mdia_box.type == "minf") {
          size_t minf_pos = mdia_box.payload;
          while (minf_pos + 8 <= mdia_box.end) {
            Mp4Box minf_box;
            if (!read_mp4_box(data, minf_pos, mdia_box.end, minf_box)) break;
            if (minf_box.type == "stbl") {
              size_t stbl_pos = minf_box.payload;
              while (stbl_pos + 8 <= minf_box.end) {
                Mp4Box stbl_box;
                if (!read_mp4_box(data, stbl_pos, minf_box.end, stbl_box)) break;
                if (stbl_box.type == "stsd") parse_stsd(data, stbl_box, track);
                stbl_pos = stbl_box.end;
              }
            }
            minf_pos = minf_box.end;
          }
        }
        mdia_pos = mdia_box.end;
      }
    }
    pos = box.end;
  }
}

Mp4InitInfo parse_mp4_init(const Bytes& data) {
  Mp4InitInfo info;
  size_t pos = 0;
  while (pos + 8 <= data.size()) {
    Mp4Box box;
    if (!read_mp4_box(data, pos, data.size(), box)) break;
    if (box.type == "moov") {
      size_t moov_pos = box.payload;
      while (moov_pos + 8 <= box.end) {
        Mp4Box child;
        if (!read_mp4_box(data, moov_pos, box.end, child)) break;
        if (child.type == "trak") {
          TrackInfo track;
          parse_trak_box(data, child, track);
          if (track.video && track.track_id >= 0 && track.codec != VideoCodec::kUnknown) {
            info.video_track_id = track.track_id;
            info.codec = track.codec;
            info.nal_length_size = track.nal_length_size;
            return info;
          }
        }
        moov_pos = child.end;
      }
    }
    pos = box.end;
  }
  fail("Could not find a video track in the fMP4 init segment");
}

struct TrunSample {
  size_t size = 0;
  size_t size_field_pos = 0;
};

struct TrafInfo {
  int track_id = -1;
  size_t base_data_offset = 0;
  bool has_base_data_offset = false;
  size_t default_sample_size = 0;
  std::vector<TrunSample> samples;
  int data_offset = 0;
  bool has_data_offset = false;
  size_t data_offset_field_pos = 0;
};

void parse_tfhd(const Bytes& data, const Mp4Box& tfhd, size_t moof_start, TrafInfo& traf) {
  if (tfhd.payload + 8 > tfhd.end) return;
  uint32_t full = be32(&data[tfhd.payload]);
  uint32_t flags = full & 0x00ffffffu;
  size_t pos = tfhd.payload + 4;
  traf.track_id = static_cast<int>(be32(&data[pos]));
  pos += 4;
  if (flags & 0x000001u) {
    if (pos + 8 > tfhd.end) return;
    traf.base_data_offset =
        static_cast<size_t>((uint64_t(be32(&data[pos])) << 32) | be32(&data[pos + 4]));
    traf.has_base_data_offset = true;
    pos += 8;
  } else if (flags & 0x020000u) {
    traf.base_data_offset = moof_start;
    traf.has_base_data_offset = true;
  }
  if (flags & 0x000002u) pos += 4;
  if (flags & 0x000008u) pos += 4;
  if (flags & 0x000010u) {
    if (pos + 4 > tfhd.end) return;
    traf.default_sample_size = be32(&data[pos]);
    pos += 4;
  }
}

void parse_trun(const Bytes& data, const Mp4Box& trun, TrafInfo& traf) {
  if (trun.payload + 8 > trun.end) return;
  uint32_t full = be32(&data[trun.payload]);
  uint32_t flags = full & 0x00ffffffu;
  size_t pos = trun.payload + 4;
  uint32_t sample_count = be32(&data[pos]);
  pos += 4;
  if (flags & 0x000001u) {
    if (pos + 4 > trun.end) return;
    traf.data_offset = static_cast<int32_t>(be32(&data[pos]));
    traf.has_data_offset = true;
    traf.data_offset_field_pos = pos;
    pos += 4;
  }
  if (flags & 0x000004u) pos += 4;
  bool has_duration = flags & 0x000100u;
  bool has_size = flags & 0x000200u;
  bool has_flags = flags & 0x000400u;
  bool has_cts = flags & 0x000800u;
  for (uint32_t i = 0; i < sample_count; ++i) {
    if (has_duration) pos += 4;
    size_t sample_size = traf.default_sample_size;
    size_t sample_size_pos = 0;
    if (has_size) {
      if (pos + 4 > trun.end) return;
      sample_size_pos = pos;
      sample_size = be32(&data[pos]);
      pos += 4;
    }
    if (has_flags) pos += 4;
    if (has_cts) pos += 4;
    traf.samples.push_back({sample_size, sample_size_pos});
  }
}

Bytes decrypt_mp4_sample_nals(const Bytes& data, size_t start, size_t end,
                              const Mp4InitInfo& init, TxsddChacha20V2& cipher,
                              DecryptStats& stats) {
  Bytes out;
  if (end <= start) return out;
  out.reserve(end - start);
  size_t pos = start;
  while (pos + static_cast<size_t>(init.nal_length_size) <= end) {
    uint32_t nal_size = 0;
    for (int i = 0; i < init.nal_length_size; ++i) {
      nal_size = (nal_size << 8) | data[pos + static_cast<size_t>(i)];
    }
    size_t nal_start = pos + static_cast<size_t>(init.nal_length_size);
    if (nal_size == 0) {
      for (int i = 0; i < init.nal_length_size; ++i) out.push_back(0);
      pos = nal_start;
      continue;
    }
    if (nal_start + nal_size > end) fail("fMP4 NAL exceeds sample bounds");
    Bytes nal(data.begin() + static_cast<std::ptrdiff_t>(nal_start),
              data.begin() + static_cast<std::ptrdiff_t>(nal_start + nal_size));
    Bytes repl = decrypt_selected_video_nal(nal, init.codec, cipher, stats);
    if (repl.size() > 0xffffffffu) fail("fMP4 NAL is too large after decrypt");
    uint32_t new_size = static_cast<uint32_t>(repl.size());
    for (int i = init.nal_length_size - 1; i >= 0; --i)
      out.push_back(static_cast<uint8_t>(new_size >> (i * 8)));
    out.insert(out.end(), repl.begin(), repl.end());
    pos = nal_start + nal_size;
  }
  if (pos != end) fail("fMP4 sample has trailing bytes after NAL parsing");
  return out;
}

Bytes decrypt_mp4_sample_nals_preserve_size(const Bytes& data, size_t start, size_t end,
                                            const Mp4InitInfo& init,
                                            TxsddChacha20V2& cipher,
                                            DecryptStats& stats) {
  if (end <= start) return {};
  Bytes out(data.begin() + static_cast<std::ptrdiff_t>(start),
            data.begin() + static_cast<std::ptrdiff_t>(end));
  size_t pos = 0;
  while (pos + static_cast<size_t>(init.nal_length_size) <= out.size()) {
    uint32_t nal_size = 0;
    for (int i = 0; i < init.nal_length_size; ++i)
      nal_size = (nal_size << 8) | out[pos + static_cast<size_t>(i)];
    size_t nal_start = pos + static_cast<size_t>(init.nal_length_size);
    if (nal_size == 0) {
      pos = nal_start;
      continue;
    }
    if (nal_start + nal_size > out.size()) fail("fMP4 NAL exceeds sample bounds");
    decrypt_selected_video_nal_in_place(out, nal_start, nal_size, init.codec, cipher, stats);
    pos = nal_start + nal_size;
  }
  if (pos != out.size()) fail("fMP4 sample has trailing bytes after NAL parsing");
  return out;
}

struct Mp4SampleRewrite {
  int track_id = -1;
  size_t original_start = 0;
  size_t original_size = 0;
  Bytes data;
  size_t sample_size_field_pos = 0;
  size_t data_offset_field_pos = 0;
  int original_data_offset = 0;
  bool has_data_offset = false;
  bool video = false;
};

Bytes rewrite_fmp4_moof_mdat(const Bytes& data, const Mp4Box& moof, const Mp4Box& mdat,
                             const Mp4InitInfo& init, TxsddChacha20V2& cipher,
                             DecryptStats& stats) {
  Bytes new_moof(data.begin() + static_cast<std::ptrdiff_t>(moof.start),
                 data.begin() + static_cast<std::ptrdiff_t>(moof.end));
  std::vector<Mp4SampleRewrite> samples;

  size_t moof_pos = moof.payload;
  while (moof_pos + 8 <= moof.end) {
    Mp4Box moof_child;
    if (!read_mp4_box(data, moof_pos, moof.end, moof_child)) break;
    if (moof_child.type == "traf") {
      TrafInfo traf;
      size_t traf_pos = moof_child.payload;
      while (traf_pos + 8 <= moof_child.end) {
        Mp4Box traf_child;
        if (!read_mp4_box(data, traf_pos, moof_child.end, traf_child)) break;
        if (traf_child.type == "tfhd")
          parse_tfhd(data, traf_child, moof.start, traf);
        else if (traf_child.type == "trun")
          parse_trun(data, traf_child, traf);
        traf_pos = traf_child.end;
      }
      if (!traf.samples.empty()) {
        size_t sample_pos = traf.has_data_offset
                                ? moof.start + static_cast<size_t>(traf.data_offset)
                                : (traf.has_base_data_offset ? traf.base_data_offset : mdat.payload);
        bool is_video = traf.track_id == init.video_track_id;
        for (const TrunSample& sample : traf.samples) {
          if (sample.size == 0) fail("fMP4 sample size is empty");
          if (sample_pos + sample.size > mdat.end) fail("fMP4 sample exceeds mdat bounds");
          Bytes sample_data;
          if (is_video) {
            if (sample.size_field_pos == 0) {
              sample_data = decrypt_mp4_sample_nals_preserve_size(
                  data, sample_pos, sample_pos + sample.size, init, cipher, stats);
            } else {
              sample_data = decrypt_mp4_sample_nals(data, sample_pos, sample_pos + sample.size,
                                                    init, cipher, stats);
            }
          } else {
            sample_data.assign(data.begin() + static_cast<std::ptrdiff_t>(sample_pos),
                               data.begin() + static_cast<std::ptrdiff_t>(sample_pos + sample.size));
          }
          samples.push_back({traf.track_id, sample_pos, sample.size, std::move(sample_data),
                             sample.size_field_pos, traf.data_offset_field_pos, traf.data_offset,
                             traf.has_data_offset, is_video});
          sample_pos += sample.size;
        }
        if (is_video) stats.pes_units += static_cast<int>(traf.samples.size());
      }
    }
    moof_pos = moof_child.end;
  }

  std::sort(samples.begin(), samples.end(),
            [](const Mp4SampleRewrite& a, const Mp4SampleRewrite& b) {
              return a.original_start < b.original_start;
            });

  Bytes new_mdat_payload;
  size_t cursor = mdat.payload;
  for (Mp4SampleRewrite& sample : samples) {
    if (sample.original_start < cursor) fail("fMP4 samples overlap inside mdat");
    new_mdat_payload.insert(new_mdat_payload.end(),
                            data.begin() + static_cast<std::ptrdiff_t>(cursor),
                            data.begin() + static_cast<std::ptrdiff_t>(sample.original_start));
    size_t new_sample_start = moof.start + new_moof.size() + 8 + new_mdat_payload.size();
    if (sample.video) {
      if (sample.sample_size_field_pos == 0 && sample.data.size() != sample.original_size)
        fail("fMP4 video trun has no sample-size field to update");
      if (sample.data.size() > 0xffffffffu) fail("fMP4 video sample is too large after decrypt");
      if (sample.sample_size_field_pos != 0) {
        write_be32(new_moof, sample.sample_size_field_pos - moof.start,
                   static_cast<uint32_t>(sample.data.size()));
      }
    }
    if (sample.has_data_offset && sample.data_offset_field_pos != 0) {
      size_t old_first = moof.start + static_cast<size_t>(sample.original_data_offset);
      if (sample.original_start == old_first) {
        int64_t new_data_offset =
            static_cast<int64_t>(new_sample_start) - static_cast<int64_t>(moof.start);
        if (new_data_offset < INT32_MIN || new_data_offset > INT32_MAX)
          fail("fMP4 data_offset exceeds int32");
        write_be32(new_moof, sample.data_offset_field_pos - moof.start,
                   static_cast<uint32_t>(static_cast<int32_t>(new_data_offset)));
      }
    }
    new_mdat_payload.insert(new_mdat_payload.end(), sample.data.begin(), sample.data.end());
    cursor = sample.original_start + sample.original_size;
  }
  if (cursor < mdat.end) {
    new_mdat_payload.insert(new_mdat_payload.end(),
                            data.begin() + static_cast<std::ptrdiff_t>(cursor),
                            data.begin() + static_cast<std::ptrdiff_t>(mdat.end));
  }

  if (new_mdat_payload.size() + 8 > 0xffffffffu) fail("fMP4 mdat is too large after decrypt");
  Bytes out;
  out.reserve(new_moof.size() + 8 + new_mdat_payload.size());
  out.insert(out.end(), new_moof.begin(), new_moof.end());
  put_be32(out, static_cast<uint32_t>(new_mdat_payload.size() + 8));
  out.insert(out.end(), {'m', 'd', 'a', 't'});
  out.insert(out.end(), new_mdat_payload.begin(), new_mdat_payload.end());
  return out;
}

DecryptStats decrypt_fmp4_fragment_in_place(Bytes& data, const Mp4InitInfo& init,
                                            TxsddChacha20V2& cipher,
                                            bool trace_enabled) {
  DecryptStats stats;
  stats.video_pid = init.video_track_id;
  stats.video_codec = codec_name(init.codec);
  Bytes rewritten;
  rewritten.reserve(data.size());
  size_t pos = 0;
  while (pos + 8 <= data.size()) {
    Mp4Box moof;
    if (!read_mp4_box(data, pos, data.size(), moof)) break;
    if (moof.type != "moof") {
      rewritten.insert(rewritten.end(), data.begin() + static_cast<std::ptrdiff_t>(pos),
                       data.begin() + static_cast<std::ptrdiff_t>(moof.end));
      pos = moof.end;
      continue;
    }
    Mp4Box mdat;
    if (!read_mp4_box(data, moof.end, data.size(), mdat) || mdat.type != "mdat")
      fail("fMP4 fragment is missing mdat after moof");
    Bytes pair = rewrite_fmp4_moof_mdat(data, moof, mdat, init, cipher, stats);
    rewritten.insert(rewritten.end(), pair.begin(), pair.end());
    pos = mdat.end;
  }
  if (pos < data.size()) {
    rewritten.insert(rewritten.end(), data.begin() + static_cast<std::ptrdiff_t>(pos), data.end());
  }
  data.swap(rewritten);
  trace(trace_enabled, std::string("[fmp4] ") + codec_label(init.codec) +
                           " track_id=" + std::to_string(init.video_track_id) +
                           " nal_length_size=" + std::to_string(init.nal_length_size));
  return stats;
}

std::vector<Segment> parse_local_playlist(const fs::path& playlist) {
  std::string text = read_text(playlist);
  auto uris = segment_uris(text);
  std::vector<Segment> segs;
  for (size_t i = 0; i < uris.size(); ++i) {
    segs.push_back({static_cast<int>(i), uris[i], uris[i],
                    basename_from_uri(uris[i], static_cast<int>(i)), false});
  }
  return segs;
}

fs::path resolve_local_segment(const fs::path& playlist_dir, const std::string& uri) {
  fs::path p(uri.substr(0, uri.find('?')));
  fs::path candidate = playlist_dir / p;
  if (fs::exists(candidate)) return candidate;
  candidate = playlist_dir / p.filename();
  if (fs::exists(candidate)) return candidate;
  fail("Could not find local segment: " + uri);
}

void verify_media(const fs::path& path, bool trace_enabled) {
  AVFormatContext* input = nullptr;
  AVDictionary* input_options = nullptr;
  av_dict_set(&input_options, "probesize", "50000000", 0);
  av_dict_set(&input_options, "analyzeduration", "10000000", 0);
  int ret = avformat_open_input(&input, path.string().c_str(), nullptr, &input_options);
  av_dict_free(&input_options);
  if (ret < 0) fail("libav verify open: " + av_error(ret));
  try {
    check_av(avformat_find_stream_info(input, nullptr), "libav verify stream info");
    int videos = 0;
    int audios = 0;
    for (unsigned i = 0; i < input->nb_streams; ++i) {
      AVMediaType type = input->streams[i]->codecpar->codec_type;
      videos += type == AVMEDIA_TYPE_VIDEO;
      audios += type == AVMEDIA_TYPE_AUDIO;
    }
    if (videos == 0) fail("libav verify: output has no video stream");
    if (trace_enabled) av_dump_format(input, 0, path.string().c_str(), 0);
    double duration = input->duration == AV_NOPTS_VALUE
                          ? 0.0
                          : static_cast<double>(input->duration) / AV_TIME_BASE;
    log_out() << "verify_libav=ok video=" << videos << " audio=" << audios
              << " duration=" << std::fixed << std::setprecision(3) << duration << "s\n";
  } catch (...) {
    avformat_close_input(&input);
    throw;
  }
  avformat_close_input(&input);
}

void remux_with_libav(const fs::path& input_path, const fs::path& output_path,
                      bool trace_enabled) {
  AVFormatContext* input = nullptr;
  AVFormatContext* output = nullptr;
  AVDictionary* input_options = nullptr;
  AVPacket* packet = av_packet_alloc();
  AVPacket* filtered = av_packet_alloc();
  std::vector<int> mapping;
  std::vector<AVBSFContext*> filters;
  bool output_open = false;
  int ret = 0;

  auto cleanup = [&] {
    av_dict_free(&input_options);
    for (AVBSFContext*& filter : filters) av_bsf_free(&filter);
    av_packet_free(&filtered);
    av_packet_free(&packet);
    avformat_close_input(&input);
    if (output && output_open && !(output->oformat->flags & AVFMT_NOFILE))
      avio_closep(&output->pb);
    avformat_free_context(output);
    output = nullptr;
  };

  try {
    if (!packet || !filtered) fail("libav: could not allocate packet");
    av_dict_set(&input_options, "probesize", "50000000", 0);
    av_dict_set(&input_options, "analyzeduration", "10000000", 0);
    check_av(avformat_open_input(&input, input_path.string().c_str(), nullptr,
                                 &input_options),
             "libav open MPEG-TS");
    av_dict_free(&input_options);
    check_av(avformat_find_stream_info(input, nullptr), "libav read MPEG-TS streams");
    if (trace_enabled) av_dump_format(input, 0, input_path.string().c_str(), 0);

    std::string ext = output_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const char* format = ext == ".mov" ? "mov" : "mp4";
    check_av(avformat_alloc_output_context2(&output, nullptr, format,
                                            output_path.string().c_str()),
             "libav create output");
    if (!output) fail("libav: output context is empty");
    output->avoid_negative_ts = AVFMT_AVOID_NEG_TS_MAKE_NON_NEGATIVE;

    mapping.assign(input->nb_streams, -1);
    filters.assign(input->nb_streams, nullptr);
    int output_index = 0;
    for (unsigned i = 0; i < input->nb_streams; ++i) {
      AVStream* in_stream = input->streams[i];
      AVCodecParameters* in_params = in_stream->codecpar;
      if (in_params->codec_type != AVMEDIA_TYPE_VIDEO &&
          in_params->codec_type != AVMEDIA_TYPE_AUDIO)
        continue;

      AVBSFContext* filter = nullptr;
      if (in_params->codec_id == AV_CODEC_ID_AAC) {
        const AVBitStreamFilter* definition = av_bsf_get_by_name("aac_adtstoasc");
        if (!definition) fail("libav: missing bitstream filter aac_adtstoasc");
        check_av(av_bsf_alloc(definition, &filter), "libav allocate AAC filter");
        filters[i] = filter;
        check_av(avcodec_parameters_copy(filter->par_in, in_params),
                 "libav copy AAC parameters");
        filter->time_base_in = in_stream->time_base;
        check_av(av_bsf_init(filter), "libav init AAC filter");
      }

      AVStream* out_stream = avformat_new_stream(output, nullptr);
      if (!out_stream) fail("libav: could not create output stream");
      mapping[i] = output_index++;
      AVCodecParameters* source_params = filter ? filter->par_out : in_params;
      check_av(avcodec_parameters_copy(out_stream->codecpar, source_params),
               "libav copy stream parameters");
      out_stream->codecpar->codec_tag = 0;
      if (out_stream->codecpar->codec_id == AV_CODEC_ID_HEVC)
        out_stream->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
      if (out_stream->codecpar->codec_id == AV_CODEC_ID_VVC)
        out_stream->codecpar->codec_tag = MKTAG('v', 'v', 'c', '1');
      out_stream->time_base = filter ? filter->time_base_out : in_stream->time_base;
    }
    if (output_index == 0) fail("libav: MPEG-TS has no audio/video stream");

    if (!(output->oformat->flags & AVFMT_NOFILE)) {
      check_av(avio_open(&output->pb, output_path.string().c_str(), AVIO_FLAG_WRITE),
               "libav open output");
      output_open = true;
    }
    AVDictionary* mux_options = nullptr;
    av_dict_set(&mux_options, "movflags", "+faststart", 0);
    int header_result = avformat_write_header(output, &mux_options);
    av_dict_free(&mux_options);
    check_av(header_result, "libav write MP4 header");
    if (trace_enabled) av_dump_format(output, 0, output_path.string().c_str(), 1);

    auto write_packet = [&](AVPacket* current, unsigned input_index,
                            AVRational source_time_base) {
      int mapped = mapping[input_index];
      AVStream* out_stream = output->streams[mapped];
      av_packet_rescale_ts(current, source_time_base, out_stream->time_base);
      current->stream_index = mapped;
      current->pos = -1;
      check_av(av_interleaved_write_frame(output, current), "libav mux packet");
    };

    while ((ret = av_read_frame(input, packet)) >= 0) {
      unsigned input_index = static_cast<unsigned>(packet->stream_index);
      if (input_index >= mapping.size() || mapping[input_index] < 0) {
        av_packet_unref(packet);
        continue;
      }
      AVBSFContext* filter = filters[input_index];
      if (!filter) {
        write_packet(packet, input_index, input->streams[input_index]->time_base);
        continue;
      }
      check_av(av_bsf_send_packet(filter, packet), "libav filter AAC packet");
      while ((ret = av_bsf_receive_packet(filter, filtered)) >= 0)
        write_packet(filtered, input_index, filter->time_base_out);
      if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        check_av(ret, "libav receive AAC packet");
    }
    if (ret != AVERROR_EOF) check_av(ret, "libav read MPEG-TS packet");

    for (unsigned i = 0; i < filters.size(); ++i) {
      AVBSFContext* filter = filters[i];
      if (!filter) continue;
      check_av(av_bsf_send_packet(filter, nullptr), "libav flush AAC filter");
      while ((ret = av_bsf_receive_packet(filter, filtered)) >= 0)
        write_packet(filtered, i, filter->time_base_out);
      if (ret != AVERROR_EOF && ret != AVERROR(EAGAIN))
        check_av(ret, "libav drain AAC filter");
    }
    check_av(av_write_trailer(output), "libav write MP4 trailer");
  } catch (...) {
    cleanup();
    throw;
  }
  cleanup();
}

bool playlist_has_encryption_key_text(const std::string& playlist_text) {
  std::istringstream ss(playlist_text);
  std::string line;
  while (std::getline(ss, line)) {
    std::string stripped = trim(line);
    if (stripped.rfind("#EXT-X-KEY:", 0) != 0 &&
        stripped.rfind("#EXT-X-SESSION-KEY:", 0) != 0)
      continue;
    std::string upper = stripped;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper.find("METHOD=NONE") == std::string::npos) return true;
  }
  return false;
}

fs::path merge_local_playlist_media(const fs::path& playlist_path, const fs::path& work_dir) {
  fs::create_directories(work_dir);
  std::string playlist_text = read_text(playlist_path);
  fs::path playlist_dir = playlist_path.parent_path();
  bool fmp4 = false;
  fs::path merged = work_dir / "merged.ts";
  if (auto init_uri = map_uri(playlist_text)) {
    fmp4 = true;
    merged = work_dir / "merged.mp4";
    fs::path init_src = resolve_local_segment(playlist_dir, *init_uri);
    Bytes init_data = read_bytes(init_src);
    if (!is_mp4_like(init_data)) fail(init_src.string() + " is not an fMP4 init segment");
  }
  std::vector<Segment> segs = parse_local_playlist(playlist_path);
  if (segs.empty()) fail("Local playlist contains no segments");

  std::ofstream out(merged, std::ios::binary);
  if (!out) fail("Unable to write merged media");
  auto append_file = [&](const fs::path& path) {
    Bytes data = read_bytes(path);
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!out) fail("Failed to write merged media");
  };
  if (fmp4) append_file(resolve_local_segment(playlist_dir, *map_uri(playlist_text)));
  for (const Segment& seg : segs) {
    fs::path src = resolve_local_segment(playlist_dir, seg.uri);
    Bytes probe = read_bytes(src);
    if (fmp4 && !is_mp4_like(probe)) fail(src.string() + " is not an fMP4 fragment");
    if (!fmp4 && !is_ts(probe)) fail(src.string() + " is not MPEG-TS");
    out.write(reinterpret_cast<const char*>(probe.data()),
              static_cast<std::streamsize>(probe.size()));
    if (!out) fail("Failed to write merged media");
  }
  return merged;
}

void remux_plain_hls(const fs::path& playlist_path, const fs::path& output_path,
                     const fs::path& work_dir, bool trace_enabled) {
  if (playlist_has_encryption_key_text(read_text(playlist_path)))
    fail("Playlist has EXT-X-KEY, but the decoded response has no DRM material");
  fs::path merged = merge_local_playlist_media(playlist_path, work_dir);
  if (!output_path.parent_path().empty()) fs::create_directories(output_path.parent_path());
  std::string ext = output_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext == ".ts") {
    if (!is_ts(read_bytes(merged)))
      fail("The .ts output format only supports plaintext MPEG-TS playlists");
    fs::copy_file(merged, output_path, fs::copy_options::overwrite_existing);
  } else if (ext == ".mp4" || ext == ".m4v" || ext == ".mov") {
    av_log_set_level(trace_enabled ? AV_LOG_INFO : AV_LOG_ERROR);
    remux_with_libav(merged, output_path, trace_enabled);
  } else {
    fail("Output suffix must be .ts, .mp4, .m4v, or .mov");
  }
  verify_media(output_path, trace_enabled);
}

void write_decrypted_fmp4_playlist(const fs::path& local_playlist,
                                   const fs::path& output_playlist,
                                   const std::map<std::string, std::string>& mapped) {
  std::string text = read_text(local_playlist);
  std::ostringstream out;
  std::istringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    std::string stripped = trim(line);
    if (stripped.rfind("#EXT-X-KEY:", 0) == 0) continue;
    if (stripped.rfind("#EXT-X-MAP:", 0) == 0) {
      std::string key = "URI=\"";
      auto pos = line.find(key);
      if (pos != std::string::npos) {
        auto value_pos = pos + key.size();
        auto end = line.find('"', value_pos);
        if (end != std::string::npos) {
          auto it = mapped.find(line.substr(value_pos, end - value_pos));
          if (it != mapped.end())
            line.replace(value_pos, end - value_pos, it->second);
        }
      }
      out << line << "\n";
      continue;
    }
    auto it = mapped.find(stripped);
    out << (it == mapped.end() ? line : it->second) << "\n";
  }
  write_text(output_playlist, out.str());
}

void decrypt_drm_fmp4_and_remux(const fs::path& playlist_path, const fs::path& output_path,
                                const fs::path& work_dir, const DrmMaterial& drm,
                                int workers, bool trace_enabled) {
  fs::create_directories(work_dir / "decrypted");
  std::string playlist_text = read_text(playlist_path);
  auto init_uri = map_uri(playlist_text);
  if (!init_uri) fail("fMP4 playlist is missing EXT-X-MAP");
  fs::path playlist_dir = playlist_path.parent_path();
  fs::path init_src = resolve_local_segment(playlist_dir, *init_uri);
  Bytes init_data = read_bytes(init_src);
  if (!is_mp4_like(init_data)) fail(init_src.string() + " is not an fMP4 init segment");
  Mp4InitInfo init = parse_mp4_init(init_data);
  if (init.codec == VideoCodec::kUnknown || init.video_track_id < 0)
    fail("Could not discover an fMP4 video track");

  std::map<std::string, std::string> mapped;
  fs::path init_dst = work_dir / "decrypted" / init_src.filename();
  write_bytes(init_dst, init_data);
  mapped[*init_uri] = "decrypted/" + init_dst.filename().string();

  std::vector<Segment> segs = parse_local_playlist(playlist_path);
  if (segs.empty()) fail("Local playlist contains no fMP4 media segments");
  std::vector<fs::path> decrypted(segs.size());
  std::atomic<int> done{0};
  parallel_for(segs.size(), workers, [&](size_t i) {
    const Segment& seg = segs[i];
    fs::path src = resolve_local_segment(playlist_dir, seg.uri);
    fs::path dst = work_dir / "decrypted" / src.filename();
    Bytes data = read_bytes(src);
    if (!is_mp4_like(data)) fail(src.string() + " is not an fMP4 fragment");
    TxsddChacha20V2 cipher(drm.key, drm.nonce);
    DecryptStats stats = decrypt_fmp4_fragment_in_place(data, init, cipher, trace_enabled);
    write_bytes(dst, data);
    decrypted[i] = dst;
    int n = ++done;
    if (n == 1 || n == static_cast<int>(segs.size()) || n % 10 == 0)
      log_out() << "DRM decrypt fMP4 " << n << "/" << segs.size()
                << " codec=" << stats.video_codec
                << " track=" << stats.video_pid
                << " samples=" << stats.pes_units
                << " nals=" << stats.transformed_nals << "\n";
  });

  for (size_t i = 0; i < segs.size(); ++i)
    mapped[segs[i].uri] = "decrypted/" + decrypted[i].filename().string();
  fs::path decrypted_playlist = work_dir / "decrypted.m3u8";
  write_decrypted_fmp4_playlist(playlist_path, decrypted_playlist, mapped);
  fs::path merged_fmp4 = work_dir / "merged.mp4";
  {
    std::ofstream merged(merged_fmp4, std::ios::binary);
    if (!merged) fail("Unable to write merged fMP4");
    auto append_file = [&](const fs::path& path) {
      Bytes bytes = read_bytes(path);
      merged.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      if (!merged) fail("Failed to write merged fMP4");
    };
    append_file(init_dst);
    for (const fs::path& path : decrypted) append_file(path);
  }

  if (!output_path.parent_path().empty()) fs::create_directories(output_path.parent_path());
  std::string ext = output_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext != ".mp4" && ext != ".m4v" && ext != ".mov")
    fail("fMP4 output currently supports only .mp4, .m4v, or .mov");
  av_log_set_level(trace_enabled ? AV_LOG_INFO : AV_LOG_ERROR);
  remux_with_libav(merged_fmp4, output_path, trace_enabled);
  verify_media(output_path, trace_enabled);
}

void decrypt_drm_and_remux(const fs::path& playlist_path, const fs::path& output_path,
                           const fs::path& work_dir, const DrmMaterial& drm,
                           int workers, bool trace_enabled) {
  if (map_uri(read_text(playlist_path))) {
    decrypt_drm_fmp4_and_remux(playlist_path, output_path, work_dir, drm,
                               workers, trace_enabled);
    return;
  }
  fs::create_directories(work_dir / "decrypted");
  fs::create_directories(work_dir / "downloads");
  std::vector<Segment> segs = parse_local_playlist(playlist_path);
  if (segs.empty()) fail("Local playlist contains no segments");
  fs::path playlist_dir = playlist_path.parent_path();
  std::vector<fs::path> decrypted(segs.size());
  std::atomic<int> done{0};
  parallel_for(segs.size(), workers, [&](size_t i) {
    const Segment& seg = segs[i];
    fs::path src = resolve_local_segment(playlist_dir, seg.uri);
    fs::path dst = work_dir / "decrypted" /
                   (std::string(4 - std::min<int>(4, std::to_string(seg.index).size()), '0') +
                    std::to_string(seg.index) + "_" + basename_from_uri(seg.uri, seg.index));
    Bytes data = read_bytes(src);
    if (!is_ts(data)) fail(src.string() + " is not MPEG-TS");
    TxsddChacha20V2 cipher(drm.key, drm.nonce);
    DecryptStats stats = decrypt_ts_in_place(data, cipher, trace_enabled);
    write_bytes(dst, data);
    decrypted[i] = dst;
    int n = ++done;
    if (n == 1 || n == static_cast<int>(segs.size()) || n % 10 == 0)
      log_out() << "DRM decrypt " << n << "/" << segs.size()
                << " codec=" << stats.video_codec
                << " pid=" << (stats.video_pid < 0 ? -1 : stats.video_pid) << "\n";
  });

  fs::path merged_ts = work_dir / "merged.ts";
  std::ofstream merged(merged_ts, std::ios::binary);
  if (!merged) fail("Unable to write merged TS");
  for (const auto& p : decrypted) {
    Bytes data = read_bytes(p);
    merged.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  merged.close();

  if (!output_path.parent_path().empty()) fs::create_directories(output_path.parent_path());
  std::string ext = output_path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (ext == ".ts") {
    fs::copy_file(merged_ts, output_path, fs::copy_options::overwrite_existing);
  } else if (ext == ".mp4" || ext == ".m4v" || ext == ".mov") {
    av_log_set_level(trace_enabled ? AV_LOG_INFO : AV_LOG_ERROR);
    remux_with_libav(merged_ts, output_path, trace_enabled);
  } else {
    fail("Output suffix must be .ts, .mp4, .m4v, or .mov");
  }
  verify_media(output_path, trace_enabled);
}

const nlohmann::json& select_video(const nlohmann::json& decoded, int index) {
  const auto& videos = decoded.at("vl").at("vi");
  if (!videos.is_array() || videos.empty()) fail("Decoded response has no vl.vi");
  if (index < 0 || index >= static_cast<int>(videos.size())) fail("video-index out of range");
  return videos[index];
}

}  // namespace

nlohmann::json decrypt_anc(const std::string& response_text,
                           const std::string& request_text,
                           bool trace_enabled,
                           const AncOptions& anc_options) {
  return decrypt_anc_text(response_text, request_text, anc_options, trace_enabled);
}

std::string detect_annexb_video_codec_for_test(const std::string& payload) {
  Bytes bytes(payload.begin(), payload.end());
  return codec_name(detect_annexb_codec(bytes));
}

std::string detect_pmt_video_codec_for_test(const std::string& section) {
  Bytes bytes(section.begin(), section.end());
  return codec_name(parse_pmt_section(bytes).codec);
}

bool response_has_anc_for_test(const std::string& response_text) {
  return response_has_anc_text(response_text);
}

bool playlist_has_encryption_key_for_test(const std::string& playlist_text) {
  return playlist_has_encryption_key_text(playlist_text);
}

size_t subtitle_track_count_for_test(const nlohmann::json& decoded) {
  return subtitle_tracks_from_sfl(decoded).size();
}

std::string merge_webvtt_segments_for_test(const std::vector<std::string>& segments) {
  return merge_webvtt_segments(segments);
}

std::string convert_subtitle_for_test(const std::string& webvtt,
                                      const std::string& format) {
  return convert_subtitle_format(webvtt, format);
}

std::string default_work_dir() {
  // Cùng gốc dữ liệu với account/config — bố cục do storage/paths.h quy định,
  // pipeline không tự đọc $HOME.
  return storage::app_home();
}

std::string cache_work_dir(const std::string& root, const std::string& vid,
                           const std::string& defn) {
  fs::path base = root.empty() ? fs::path(default_work_dir()) : fs::path(root);
  return (base / "videos" / cache_component(vid, false) /
          cache_component(defn, true))
      .lexically_normal()
      .string();
}

std::string normalize_request_vid(std::string vid) {
  std::string text = trim(vid);
  if (!text.empty() && text.front() == '{') {
    try {
      nlohmann::json j = nlohmann::json::parse(text);
      std::string nested = j.value("vid", std::string());
      if (!nested.empty()) return nested;
      nested = j.value("historyVid", std::string());
      if (!nested.empty()) return nested;
    } catch (const std::exception&) {
    }
  }
  return vid;
}

CacheIdentity cache_identity_from_request(const std::string& request_text) {
  std::string vid = normalize_request_vid(request_form_value(request_text, "vid"));
  if (vid.empty())
    vid = normalize_request_vid(request_form_value(request_text, "previd"));
  return {vid, request_form_value(request_text, "defn")};
}

int emergency_time_from_remain_seconds(int remain_seconds) {
  return 1000 * (remain_seconds / 2);
}

std::string normalize_segment_url(std::string url, int video_feature,
                                  int cdn_strategy, int emergency_time_ms) {
  // libDownloadProxy.so ClipInfo_ParseExtraInfo @ 0x2FA2A0 copies
  // (dl_param_cdn_feature >> 2) & 1; sub_2A1230 returns 10 for the captured
  // normal source branch. Ground truth:
  // ../analytics/app/response.today.decoded.auto.json:449 and
  // ../analytics/app/mdse_getplayts_trace.log:822,839.
  const int dlipscene = (video_feature & 4) != 0 ? 10 : 0;
  append_query_param(url, "cost", "low");
  if (emergency_time_ms >= 0 && emergency_time_ms <= 1000)
    append_query_param(url, "emergency_level", "1");
  append_query_param(url, "cdn_strategy", std::to_string(cdn_strategy));
  append_query_param(url, "dlipscene", std::to_string(dlipscene));
  return url;
}

int run_download(const Options& input) {
  try {
    Options opt = input;
    if (opt.workers < 1) opt.workers = 1;
    if (opt.timeout < 1) opt.timeout = 30;
    if (opt.max_buffer_length <= 0.0) opt.max_buffer_length = 30.0;
    if (opt.download_speed <= 0.0) opt.download_speed = 1.0;
    if (opt.retries < 0) opt.retries = 0;
    opt.subtitle_format = normalize_subtitle_format(opt.subtitle_format);
    if (opt.request_path.empty() || opt.response_path.empty() ||
        (!opt.no_download && opt.output_path.empty())) {
      log_err() << "download: requires --request and --response";
      if (!opt.no_download) log_err() << " and -o/--output";
      log_err() << "\n";
      return 2;
    }
    fs::path request = abs_path(opt.request_path);
    fs::path response = abs_path(opt.response_path);
    fs::path output;
    if (!opt.output_path.empty()) output = abs_path(opt.output_path);
    fs::path work_root =
        abs_path(opt.work_dir.empty() ? default_work_dir() : opt.work_dir);
    if (!fs::exists(request)) fail("Request file not found: " + request.string());
    if (!fs::exists(response)) fail("Response file not found: " + response.string());

    std::string request_text = read_text(request);
    std::string response_text = read_text(response);
    bool used_anc = false;
    nlohmann::json decoded =
        decode_getvinfo_response_text(response_text, request_text, opt.trace, opt.anc,
                                      &used_anc);
    const nlohmann::json& video = select_video(decoded, opt.video_index);
    CacheIdentity request_identity = cache_identity_from_request(request_text);
    std::string vid = !opt.vid.empty() ? opt.vid : video.value("vid", std::string());
    if (vid.empty()) vid = request_identity.vid;
    std::string defn = !opt.defn.empty() ? opt.defn : request_identity.defn;
    fs::path work = abs_path(cache_work_dir(work_root.string(), vid, defn));

    if (opt.no_download) {
      print_no_download_info(decoded, video, request_identity, used_anc, opt.video_index);
      return 0;
    }

    fs::create_directories(work);

    log_out() << "# native download\n"
              << "request=" << request << "\n"
              << "response=" << response << "\n"
              << "response_payload=" << (used_anc ? "anc" : "plaintext") << "\n"
              << "output=" << output << "\n"
              << "work_dir=" << work << "\n";

    write_text(work / "decoded.json", decoded.dump(2) + "\n");
    std::optional<DrmMaterial> drm = maybe_extract_drm_material(decoded);
    if (drm)
      log_out() << "DRM key=" << drm->key << " nonce=" << drm->nonce << "\n";
    else
      log_out() << "DRM material=none (plaintext media expected)\n";

    fs::path hls_dir = work / "hls";
    fs::path drm_dir = work / "drm";
    download_proxy::Context config_context = opt.proxy_config_context;
    if (config_context.version.empty()) config_context.version = opt.download_user_agent;
    if (config_context.platform.empty())
      config_context.platform = request_form_value(request_text, "platform");
    if (config_context.guid.empty())
      config_context.guid = request_form_value(request_text, "guid");
    if (config_context.qimei36.empty())
      config_context.qimei36 = request_form_value(request_text, "qimei");
    if (config_context.device_model.empty())
      config_context.device_model = request_form_value(request_text, "devicemodel");

    download_proxy::Config proxy_config;
    try {
      proxy_config = download_proxy::fetch_config(
          config_context, opt.timeout, opt.insecure, opt.proxy_url);
      write_text(work / "download_proxy_config.json",
                 proxy_config.raw_response + "\n");
      log_out() << "download_proxy_config cdn_strategy="
                << proxy_config.cdn_strategy
                << " quick=" << proxy_config.quick_download_cdn_strategy
                << " source=appcfg\n";
      if (proxy_config.quick_download_cdn_strategy >= 1)
        log_err() << "download_proxy_config: QuickDownloadCDNStrategy is enabled; "
                     "CLI falls back to CDNStrategy because native CDN bandwidth state is unavailable\n";
    } catch (const std::exception& e) {
      log_err() << "download_proxy_config failed, use native defaults: "
                << e.what() << "\n";
    }

    log_out() << "\n# download HLS segments\n";
    download_hls_segments(video, hls_dir, request_text, opt.download_user_agent,
                          proxy_config.cdn_strategy, opt.workers, opt.timeout,
                          opt.retries, opt.getplay_sleep_sec, opt.remain_seconds,
                          opt.segment_query, opt.simulate_player_buffer,
                          opt.max_buffer_length, opt.download_speed, opt.trace,
                          !opt.workers_explicit);

    if (drm) {
      log_out() << "\n# DRM decrypt and remux\n";
      decrypt_drm_and_remux(hls_dir / "local.m3u8", output, drm_dir, *drm,
                            opt.workers, opt.trace);
    } else {
      log_out() << "\n# plaintext remux\n";
      remux_plain_hls(hls_dir / "local.m3u8", output, work / "plain", opt.trace);
    }

    if (opt.download_subtitle) {
      log_out() << "\n# subtitle download\n";
      download_subtitles(decoded, request_text, work, output, opt.download_user_agent,
                         opt.timeout, opt.retries, opt.subtitle_format, opt.trace);
    }

    log_out() << "\nOK: " << output << "\nwork_dir: " << work << "\n";
    return 0;
  } catch (const std::exception& e) {
    log_err() << "download failed: " << e.what() << "\n";
    return 1;
  }
}

}  // namespace tencent::download
