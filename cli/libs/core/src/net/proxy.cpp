// proxy.cpp - SIP002 parser and TCP Shadowsocks AEAD client bridge.
#include "tencent/core/net/proxy.h"

#include "tencent/platform/socket.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace tencent::proxy {
namespace {

using Bytes = std::vector<uint8_t>;

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         std::equal(prefix.begin(), prefix.end(), value.begin());
}

int hex_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string percent_decode(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] != '%') {
      out.push_back(value[i]);
      continue;
    }
    if (i + 2 >= value.size()) throw std::invalid_argument("proxy: invalid percent-encoding");
    int hi = hex_value(value[i + 1]), lo = hex_value(value[i + 2]);
    if (hi < 0 || lo < 0) throw std::invalid_argument("proxy: invalid percent-encoding");
    out.push_back(static_cast<char>((hi << 4) | lo));
    i += 2;
  }
  return out;
}

Bytes base64url_decode(std::string value) {
  for (char& c : value) {
    if (c == '-') c = '+';
    else if (c == '_') c = '/';
    else if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '+' || c == '/' || c == '='))
      throw std::invalid_argument("proxy: invalid SIP002 base64url userinfo");
  }
  if (value.size() % 4 == 1)
    throw std::invalid_argument("proxy: invalid SIP002 base64url userinfo");
  while (value.size() % 4) value.push_back('=');

  Bytes out(value.size() / 4 * 3);
  int n = EVP_DecodeBlock(out.data(),
                          reinterpret_cast<const unsigned char*>(value.data()),
                          static_cast<int>(value.size()));
  if (n < 0) throw std::invalid_argument("proxy: invalid SIP002 base64url userinfo");
  if (!value.empty() && value.back() == '=') --n;
  if (value.size() >= 2 && value[value.size() - 2] == '=') --n;
  out.resize(static_cast<std::size_t>(n));
  return out;
}

bool supported_method(const std::string& method) {
  return method == "aes-128-gcm" || method == "aes-256-gcm" ||
         method == "chacha20-ietf-poly1305";
}

void parse_host_port(const std::string& authority, std::string& host, uint16_t& port) {
  std::string port_text;
  if (!authority.empty() && authority.front() == '[') {
    std::size_t close = authority.find(']');
    if (close == std::string::npos || close + 1 >= authority.size() ||
        authority[close + 1] != ':')
      throw std::invalid_argument("proxy: IPv6 addresses must use [host]:port");
    host = authority.substr(1, close - 1);
    port_text = authority.substr(close + 2);
  } else {
    std::size_t colon = authority.rfind(':');
    if (colon == std::string::npos)
      throw std::invalid_argument("proxy: missing Shadowsocks port");
    host = authority.substr(0, colon);
    port_text = authority.substr(colon + 1);
  }
  if (host.empty() || port_text.empty())
    throw std::invalid_argument("proxy: invalid Shadowsocks host/port");
  std::size_t used = 0;
  unsigned long parsed = 0;
  try {
    parsed = std::stoul(port_text, &used, 10);
  } catch (...) {
    throw std::invalid_argument("proxy: invalid Shadowsocks port");
  }
  if (used != port_text.size() || parsed == 0 || parsed > 65535)
    throw std::invalid_argument("proxy: invalid Shadowsocks port");
  port = static_cast<uint16_t>(parsed);
}

void parse_query(const std::string& query, ProxySpec& spec) {
  std::size_t start = 0;
  while (start <= query.size()) {
    std::size_t end = query.find('&', start);
    if (end == std::string::npos) end = query.size();
    std::string part = query.substr(start, end - start);
    std::size_t eq = part.find('=');
    std::string key = percent_decode(part.substr(0, eq));
    std::string value = eq == std::string::npos ? "" : percent_decode(part.substr(eq + 1));
    if (key == "plugin") spec.plugin = value;
    if (end == query.size()) break;
    start = end + 1;
  }
}

// send_all / recv_exact / connect_tcp ĐÃ CHUYỂN sang lớp platform
// (tencent/platform/socket.h) — lõi không gọi thẳng syscall socket nữa.
using platform::SocketHandle;
using platform::kInvalidSocket;
using platform::connect_tcp;
using platform::recv_exact;
using platform::send_all;

Bytes evp_bytes_to_key(const std::string& password, std::size_t key_size) {
  Bytes key, previous;
  key.reserve(key_size);
  while (key.size() < key_size) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("OpenSSL EVP_MD_CTX_new failed");
    std::array<uint8_t, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    bool ok = EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1 &&
              (previous.empty() ||
               EVP_DigestUpdate(ctx, previous.data(), previous.size()) == 1) &&
              EVP_DigestUpdate(ctx, password.data(), password.size()) == 1 &&
              EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) throw std::runtime_error("OpenSSL MD5 key derivation failed");
    previous.assign(digest.begin(), digest.begin() + digest_len);
    std::size_t take = std::min(previous.size(), key_size - key.size());
    key.insert(key.end(), previous.begin(), previous.begin() + take);
  }
  return key;
}

Bytes hmac_sha1(const Bytes& key, const Bytes& data) {
  std::array<uint8_t, EVP_MAX_MD_SIZE> digest{};
  unsigned int size = 0;
  if (!HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()), data.data(), data.size(),
            digest.data(), &size))
    throw std::runtime_error("OpenSSL HMAC-SHA1 failed");
  return Bytes(digest.begin(), digest.begin() + size);
}

Bytes hkdf_sha1(const Bytes& master, const Bytes& salt, std::size_t output_size) {
  Bytes prk = hmac_sha1(salt, master);
  static const uint8_t kInfo[] = {'s', 's', '-', 's', 'u', 'b', 'k', 'e', 'y'};
  Bytes output, previous;
  uint8_t counter = 1;
  while (output.size() < output_size) {
    Bytes input = previous;
    input.insert(input.end(), std::begin(kInfo), std::end(kInfo));
    input.push_back(counter++);
    previous = hmac_sha1(prk, input);
    std::size_t take = std::min(previous.size(), output_size - output.size());
    output.insert(output.end(), previous.begin(), previous.begin() + take);
  }
  return output;
}

struct CipherInfo {
  const EVP_CIPHER* cipher = nullptr;
  std::size_t key_size = 0;
};

CipherInfo cipher_info(const std::string& method) {
  if (method == "aes-128-gcm") return {EVP_aes_128_gcm(), 16};
  if (method == "aes-256-gcm") return {EVP_aes_256_gcm(), 32};
  if (method == "chacha20-ietf-poly1305") return {EVP_chacha20_poly1305(), 32};
  throw std::invalid_argument("proxy: unsupported Shadowsocks cipher: " + method);
}

void increment_nonce(std::array<uint8_t, 12>& nonce) {
  for (uint8_t& byte : nonce) {
    if (++byte != 0) break;
  }
}

class AeadState {
 public:
  AeadState(const std::string& method, const std::string& password, const Bytes& salt)
      : info_(cipher_info(method)),
        key_(hkdf_sha1(evp_bytes_to_key(password, info_.key_size), salt, info_.key_size)) {}

  Bytes encrypt(const uint8_t* plain, std::size_t size) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("OpenSSL cipher context failed");
    Bytes out(size + 16);
    int written = 0, final_size = 0;
    bool ok = EVP_EncryptInit_ex(ctx, info_.cipher, nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                  static_cast<int>(nonce_.size()), nullptr) == 1 &&
              EVP_EncryptInit_ex(ctx, nullptr, nullptr, key_.data(), nonce_.data()) == 1 &&
              EVP_EncryptUpdate(ctx, out.data(), &written, plain, static_cast<int>(size)) == 1 &&
              EVP_EncryptFinal_ex(ctx, out.data() + written, &final_size) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, 16,
                                  out.data() + written + final_size) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) throw std::runtime_error("Shadowsocks AEAD encryption failed");
    out.resize(static_cast<std::size_t>(written + final_size + 16));
    increment_nonce(nonce_);
    return out;
  }

  Bytes decrypt(const uint8_t* cipher, std::size_t size) {
    if (size < 16) throw std::runtime_error("Shadowsocks AEAD chunk is too short");
    std::size_t cipher_size = size - 16;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw std::runtime_error("OpenSSL cipher context failed");
    Bytes out(cipher_size);
    int written = 0, final_size = 0;
    bool ok = EVP_DecryptInit_ex(ctx, info_.cipher, nullptr, nullptr, nullptr) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN,
                                  static_cast<int>(nonce_.size()), nullptr) == 1 &&
              EVP_DecryptInit_ex(ctx, nullptr, nullptr, key_.data(), nonce_.data()) == 1 &&
              EVP_DecryptUpdate(ctx, out.data(), &written, cipher,
                                static_cast<int>(cipher_size)) == 1 &&
              EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, 16,
                                  const_cast<uint8_t*>(cipher + cipher_size)) == 1 &&
              EVP_DecryptFinal_ex(ctx, out.data() + written, &final_size) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) throw std::runtime_error("Shadowsocks AEAD authentication failed");
    out.resize(static_cast<std::size_t>(written + final_size));
    increment_nonce(nonce_);
    return out;
  }

 private:
  CipherInfo info_;
  Bytes key_;
  std::array<uint8_t, 12> nonce_{};
};

class AeadWriter {
 public:
  AeadWriter(SocketHandle fd, const ProxySpec& spec) : fd_(fd) {
    std::size_t salt_size = cipher_info(spec.method).key_size;
    Bytes salt(salt_size);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
      throw std::runtime_error("OpenSSL RAND_bytes failed");
    state_ = std::make_unique<AeadState>(spec.method, spec.password, salt);
    if (!send_all(fd_, salt.data(), salt.size()))
      throw std::runtime_error("Could not send Shadowsocks salt");
  }

  void write_chunk(const uint8_t* plain, std::size_t size) {
    if (size > 0x3FFF) throw std::runtime_error("Shadowsocks payload exceeds 0x3fff");
    uint8_t length[2] = {static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)};
    Bytes encrypted_length = state_->encrypt(length, sizeof(length));
    Bytes encrypted_payload = state_->encrypt(plain, size);
    if (!send_all(fd_, encrypted_length.data(), encrypted_length.size()) ||
        !send_all(fd_, encrypted_payload.data(), encrypted_payload.size()))
      throw std::runtime_error("Shadowsocks connection closed while sending");
  }

 private:
  SocketHandle fd_;
  std::unique_ptr<AeadState> state_;
};

class AeadReader {
 public:
  AeadReader(SocketHandle fd, const ProxySpec& spec) : fd_(fd) {
    std::size_t salt_size = cipher_info(spec.method).key_size;
    Bytes salt(salt_size);
    if (!recv_exact(fd_, salt.data(), salt.size()))
      throw std::runtime_error("Could not receive Shadowsocks response salt");
    state_ = std::make_unique<AeadState>(spec.method, spec.password, salt);
  }

  bool read_chunk(Bytes& plain) {
    std::array<uint8_t, 18> length_block{};
    // recv_some đã tự thử lại khi bị tín hiệu cắt ngang (EINTR), nên ở đây
    // 0 = phía kia đóng, <0 = lỗi thật.
    const long first = platform::recv_some(fd_, length_block.data(), 1);
    if (first == 0) return false;
    if (first != 1 ||
        !recv_exact(fd_, length_block.data() + 1, length_block.size() - 1))
      throw std::runtime_error("Shadowsocks response length was truncated");
    Bytes length = state_->decrypt(length_block.data(), length_block.size());
    if (length.size() != 2) throw std::runtime_error("Invalid Shadowsocks response length");
    std::size_t size = (static_cast<std::size_t>(length[0]) << 8) | length[1];
    if (size > 0x3FFF) throw std::runtime_error("Shadowsocks response payload is too large");
    Bytes block(size + 16);
    if (!recv_exact(fd_, block.data(), block.size()))
      throw std::runtime_error("Shadowsocks response payload was truncated");
    plain = state_->decrypt(block.data(), block.size());
    return true;
  }

 private:
  SocketHandle fd_;
  std::unique_ptr<AeadState> state_;
};

Bytes read_socks_target(SocketHandle client_fd) {
  uint8_t hello[2];
  if (!recv_exact(client_fd, hello, sizeof(hello)) || hello[0] != 5 || hello[1] == 0)
    throw std::runtime_error("Invalid SOCKS5 greeting");
  Bytes methods(hello[1]);
  if (!recv_exact(client_fd, methods.data(), methods.size()))
    throw std::runtime_error("SOCKS5 greeting was truncated");
  if (std::find(methods.begin(), methods.end(), uint8_t{0}) == methods.end())
    throw std::runtime_error("SOCKS5 client does not support no-auth");
  const uint8_t hello_reply[] = {5, 0};
  if (!send_all(client_fd, hello_reply, sizeof(hello_reply)))
    throw std::runtime_error("Could not send SOCKS5 greeting");

  uint8_t request[4];
  if (!recv_exact(client_fd, request, sizeof(request)) || request[0] != 5 ||
      request[1] != 1 || request[2] != 0)
    throw std::runtime_error("Invalid SOCKS5 CONNECT request");

  Bytes target = {request[3]};
  std::size_t address_size = 0;
  if (request[3] == 1) {
    address_size = 4;
  } else if (request[3] == 4) {
    address_size = 16;
  } else if (request[3] == 3) {
    uint8_t domain_size = 0;
    if (!recv_exact(client_fd, &domain_size, 1) || domain_size == 0)
      throw std::runtime_error("Invalid SOCKS5 domain");
    target.push_back(domain_size);
    address_size = domain_size;
  } else {
    throw std::runtime_error("Unsupported SOCKS5 address type");
  }
  std::size_t old_size = target.size();
  target.resize(old_size + address_size + 2);
  if (!recv_exact(client_fd, target.data() + old_size, address_size + 2))
    throw std::runtime_error("SOCKS5 target was truncated");
  return target;
}

}  // namespace

ProxySpec parse_proxy_url(const std::string& url) {
  ProxySpec spec;
  spec.original_url = url;
  if (url.empty()) return spec;

  if (!starts_with(url, "ss://")) {
    static const char* kCurlSchemes[] = {
        "http://", "https://", "socks4://", "socks4a://", "socks5://", "socks5h://",
    };
    bool supported = false;
    for (const char* scheme : kCurlSchemes) supported = supported || starts_with(url, scheme);
    if (!supported) throw std::invalid_argument("proxy: unsupported scheme");
    spec.kind = ProxyKind::kCurl;
    return spec;
  }

  spec.kind = ProxyKind::kShadowsocks;
  std::string rest = url.substr(5);
  std::size_t hash = rest.find('#');
  if (hash != std::string::npos) {
    spec.tag = percent_decode(rest.substr(hash + 1));
    rest.resize(hash);
  }
  std::size_t query = rest.find('?');
  if (query != std::string::npos) {
    parse_query(rest.substr(query + 1), spec);
    rest.resize(query);
  }
  if (!rest.empty() && rest.back() == '/') rest.pop_back();

  std::size_t at = rest.rfind('@');
  if (at == std::string::npos)
    throw std::invalid_argument("proxy: SIP002 requires userinfo@host:port");
  std::string userinfo = rest.substr(0, at);
  parse_host_port(rest.substr(at + 1), spec.host, spec.port);

  std::string credentials;
  if (userinfo.find(':') != std::string::npos) {
    credentials = percent_decode(userinfo);
  } else {
    Bytes decoded = base64url_decode(percent_decode(userinfo));
    credentials.assign(decoded.begin(), decoded.end());
  }
  std::size_t colon = credentials.find(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= credentials.size())
    throw std::invalid_argument("proxy: SIP002 userinfo must be method:password");
  spec.method = credentials.substr(0, colon);
  spec.password = credentials.substr(colon + 1);

  if (!supported_method(spec.method)) {
    if (starts_with(spec.method, "2022-"))
      throw std::invalid_argument("proxy: AEAD-2022 ciphers are not supported");
    throw std::invalid_argument("proxy: unsupported Shadowsocks cipher: " + spec.method);
  }
  if (!spec.plugin.empty())
    throw std::invalid_argument("proxy: SIP003 plugins are not supported: " + spec.plugin);
  return spec;
}

struct LocalShadowsocksProxy::Impl {
  explicit Impl(const ProxySpec& input, long timeout) : spec(input), connect_timeout_sec(timeout) {
    if (spec.kind != ProxyKind::kShadowsocks)
      throw std::invalid_argument("LocalShadowsocksProxy requires an ss:// URL");
    platform::socket_startup();
    // Cổng do hệ thống cấp, chỉ nghe trên loopback (bí mật không rời máy).
    listen_fd = platform::listen_loopback(local_port);
    worker = std::thread([this] { run(); });
  }

  ~Impl() {
    stopping = true;
    if (listen_fd != kInvalidSocket) {
      platform::shutdown_both(listen_fd);   // đánh thức accept() đang chờ
      platform::close_socket(listen_fd);
      listen_fd = kInvalidSocket;
    }
    const SocketHandle client = client_fd.load();
    const SocketHandle remote = remote_fd.load();
    if (client != kInvalidSocket) platform::shutdown_both(client);
    if (remote != kInvalidSocket) platform::shutdown_both(remote);
    if (worker.joinable()) worker.join();
  }

  void set_error(const std::string& value) {
    std::lock_guard<std::mutex> lock(error_mutex);
    if (error_text.empty() && !stopping) error_text = value;
  }

  std::string get_error() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_text;
  }

  void relay_response() {
    try {
      AeadReader reader(remote_fd.load(), spec);
      Bytes plain;
      while (!stopping && reader.read_chunk(plain)) {
        if (!send_all(client_fd.load(), plain.data(), plain.size()))
          throw std::runtime_error("SOCKS5 client closed while receiving response");
      }
    } catch (const std::exception& e) {
      set_error(e.what());
    }
    const SocketHandle client = client_fd.load();
    if (client != kInvalidSocket) platform::shutdown_write(client);
  }

  void run() {
    try {
      const SocketHandle client = platform::accept_connection(listen_fd);
      if (client == kInvalidSocket) {
        if (!stopping) throw std::runtime_error("local SOCKS5 accept failed");
        return;
      }
      client_fd = client;
      Bytes target = read_socks_target(client);

      const SocketHandle remote = connect_tcp(spec.host, spec.port, connect_timeout_sec);
      remote_fd = remote;
      AeadWriter writer(remote, spec);
      writer.write_chunk(target.data(), target.size());

      const uint8_t success[] = {5, 0, 0, 1, 0, 0, 0, 0, 0, 0};
      if (!send_all(client, success, sizeof(success)))
        throw std::runtime_error("Could not send SOCKS5 CONNECT response");

      std::thread response_thread([this] { relay_response(); });
      try {
        std::array<uint8_t, 0x3FFF> buffer{};
        while (!stopping) {
          const long n = platform::recv_some(client, buffer.data(), buffer.size());
          if (n <= 0) break;
          writer.write_chunk(buffer.data(), static_cast<std::size_t>(n));
        }
      } catch (const std::exception& e) {
        set_error(e.what());
      }
      platform::shutdown_write(remote);
      response_thread.join();
    } catch (const std::exception& e) {
      set_error(e.what());
    }

    const SocketHandle client = client_fd.exchange(kInvalidSocket);
    const SocketHandle remote = remote_fd.exchange(kInvalidSocket);
    if (client != kInvalidSocket) platform::close_socket(client);
    if (remote != kInvalidSocket) platform::close_socket(remote);
  }

  ProxySpec spec;
  long connect_timeout_sec = 20;
  SocketHandle listen_fd = kInvalidSocket;
  uint16_t local_port = 0;
  std::atomic<SocketHandle> client_fd{kInvalidSocket};
  std::atomic<SocketHandle> remote_fd{kInvalidSocket};
  std::atomic<bool> stopping{false};
  std::thread worker;
  mutable std::mutex error_mutex;
  std::string error_text;
};

LocalShadowsocksProxy::LocalShadowsocksProxy(const ProxySpec& spec, long connect_timeout_sec)
    : impl_(std::make_unique<Impl>(spec, connect_timeout_sec)) {}

LocalShadowsocksProxy::~LocalShadowsocksProxy() = default;

std::string LocalShadowsocksProxy::curl_proxy_url() const {
  return "socks5h://127.0.0.1:" + std::to_string(impl_->local_port);
}

std::string LocalShadowsocksProxy::error() const {
  return impl_->get_error();
}

}  // namespace tencent::proxy
