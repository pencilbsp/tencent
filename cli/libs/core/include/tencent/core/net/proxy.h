// proxy.h - Parse proxy URLs and provide a one-shot local Shadowsocks bridge.
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace tencent::proxy {

enum class ProxyKind {
  kNone,
  kCurl,
  kShadowsocks,
};

struct ProxySpec {
  ProxyKind kind = ProxyKind::kNone;
  std::string original_url;
  std::string method;
  std::string password;
  std::string host;
  uint16_t port = 0;
  std::string plugin;
  std::string tag;
};

// Supports libcurl proxy schemes and SIP002 ss:// URLs. Throws invalid_argument
// for malformed URLs and unsupported Shadowsocks methods/plugins.
ProxySpec parse_proxy_url(const std::string& url);

// Runs a single-connection SOCKS5 listener on 127.0.0.1 and relays it through
// the Shadowsocks server. The URL returned by curl_proxy_url() is safe to pass
// to CURLOPT_PROXY. Secrets are never included in that URL.
class LocalShadowsocksProxy {
 public:
  explicit LocalShadowsocksProxy(const ProxySpec& spec, long connect_timeout_sec = 20);
  ~LocalShadowsocksProxy();

  LocalShadowsocksProxy(const LocalShadowsocksProxy&) = delete;
  LocalShadowsocksProxy& operator=(const LocalShadowsocksProxy&) = delete;

  std::string curl_proxy_url() const;
  std::string error() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tencent::proxy
