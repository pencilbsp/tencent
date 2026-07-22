// test_http_client.cpp — Smoke test cho http_client (Pha D).
//
// http_client KHÔNG byte-exact (transport). Test: gzip roundtrip + interop với
// gzip do Python sinh (generated_vectors_acct.h) + maybe_gunzip passthrough.
// (Không gọi mạng — verify live ở Pha E.)
#include <cstdio>
#include <string>

#include "vectors/generated_vectors_acct.h"
#include "tencent/core/net/http_client.h"
#include "tencent/core/net/proxy.h"

namespace http = tencent::http;

static int g_failures = 0;
static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}
static std::string from_hex(const std::string& hex) {
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<char>((nib(hex[i]) << 4) | nib(hex[i + 1])));
  return out;
}

int main() {
  std::printf("== Pha D: http_client ==\n");

  // gzip roundtrip nội bộ.
  std::string x;
  for (int i = 0; i < 1000; ++i) x.push_back(static_cast<char>((i * 7) & 0xFF));
  check(http::gunzip(http::gzip_compress(x)) == x, "gunzip(gzip_compress(x)) == x");

  // interop: giải blob gzip do Python sinh.
  std::string plain = from_hex(vacc::GZ_PLAIN_HEX);
  std::string blob = from_hex(vacc::GZ_BLOB_HEX);
  check(http::gunzip(blob) == plain, "gunzip(python_gzip_blob) == plain");
  check(http::maybe_gunzip(blob) == plain, "maybe_gunzip(gzip) -> giải nén");

  // maybe_gunzip passthrough cho dữ liệu không gzip.
  std::string not_gz = "khong-phai-gzip";
  check(http::maybe_gunzip(not_gz) == not_gz, "maybe_gunzip(non-gzip) -> nguyên bản");

  // SIP002: base64url(userinfo) và plain userinfo percent-encoded.
  auto encoded = tencent::proxy::parse_proxy_url(
      "ss://YWVzLTI1Ni1nY206cEBzczp3b3Jk@example.com:8388#Tokyo%201");
  check(encoded.kind == tencent::proxy::ProxyKind::kShadowsocks,
        "SIP002 base64url -> Shadowsocks");
  check(encoded.method == "aes-256-gcm" && encoded.password == "p@ss:word" &&
            encoded.host == "example.com" && encoded.port == 8388 &&
            encoded.tag == "Tokyo 1",
        "SIP002 base64url fields");

  auto plain_proxy = tencent::proxy::parse_proxy_url(
      "ss://aes-128-gcm:p%40ss%3Aword@[2001:db8::1]:443/#IPv6");
  check(plain_proxy.method == "aes-128-gcm" && plain_proxy.password == "p@ss:word" &&
            plain_proxy.host == "2001:db8::1" && plain_proxy.port == 443,
        "SIP002 plain percent-encoded + IPv6");

  auto socks = tencent::proxy::parse_proxy_url("socks5h://127.0.0.1:1080");
  check(socks.kind == tencent::proxy::ProxyKind::kCurl &&
            socks.original_url == "socks5h://127.0.0.1:1080",
        "libcurl proxy passthrough");

  bool plugin_rejected = false;
  try {
    (void)tencent::proxy::parse_proxy_url(
        "ss://YWVzLTI1Ni1nY206cGFzcw@example.com:8388/?plugin=v2ray-plugin%3Btls");
  } catch (const std::invalid_argument&) {
    plugin_rejected = true;
  }
  check(plugin_rejected, "SIP003 plugin bị từ chối rõ ràng");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
