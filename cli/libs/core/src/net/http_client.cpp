// http_client.cpp — gzip (zlib) + HTTP POST (libcurl). Xem http_client.h.
#include "tencent/core/net/http_client.h"

#include <memory>
#include <stdexcept>

#include <curl/curl.h>
#include <zlib.h>

#include "tencent/core/net/proxy.h"

namespace tencent::http {

// ── gzip (zlib windowBits 15+16) ─────────────────────────────────────────────
std::string gzip_compress(const std::string& data) {
  z_stream zs{};
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK)
    throw std::runtime_error("deflateInit2 failed");
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
  zs.avail_in = static_cast<uInt>(data.size());
  std::string out;
  char buf[16384];
  int ret;
  do {
    zs.next_out = reinterpret_cast<Bytef*>(buf);
    zs.avail_out = sizeof(buf);
    ret = deflate(&zs, Z_FINISH);
    out.append(buf, sizeof(buf) - zs.avail_out);
  } while (ret == Z_OK);
  deflateEnd(&zs);
  if (ret != Z_STREAM_END) throw std::runtime_error("deflate failed");
  return out;
}

std::string gunzip(const std::string& data) {
  z_stream zs{};
  if (inflateInit2(&zs, 15 + 16) != Z_OK) throw std::runtime_error("inflateInit2 failed");
  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
  zs.avail_in = static_cast<uInt>(data.size());
  std::string out;
  char buf[16384];
  int ret;
  do {
    zs.next_out = reinterpret_cast<Bytef*>(buf);
    zs.avail_out = sizeof(buf);
    ret = inflate(&zs, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
      inflateEnd(&zs);
      throw std::runtime_error("inflate failed");
    }
    out.append(buf, sizeof(buf) - zs.avail_out);
  } while (ret != Z_STREAM_END);
  inflateEnd(&zs);
  return out;
}

// ../analytics/app/login_refresh_transport.py:68-74
std::string maybe_gunzip(const std::string& data) {
  if (data.size() >= 2 && static_cast<uint8_t>(data[0]) == 0x1F &&
      static_cast<uint8_t>(data[1]) == 0x8B) {
    try {
      return gunzip(data);
    } catch (const std::exception&) {
      return data;
    }
  }
  return data;
}

// ── HTTP POST (libcurl) ──────────────────────────────────────────────────────
static std::size_t write_cb(char* ptr, std::size_t size, std::size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

std::pair<long, std::string> http_post(const std::string& url, const OrderedKV& headers,
                                       const std::string& body, long timeout_sec,
                                       bool insecure, const std::string& proxy_url) {
  proxy::ProxySpec proxy_spec = proxy::parse_proxy_url(proxy_url);
  std::unique_ptr<proxy::LocalShadowsocksProxy> ss_proxy;
  std::string curl_proxy;
  if (proxy_spec.kind == proxy::ProxyKind::kShadowsocks) {
    ss_proxy = std::make_unique<proxy::LocalShadowsocksProxy>(proxy_spec, timeout_sec);
    curl_proxy = ss_proxy->curl_proxy_url();
  } else if (proxy_spec.kind == proxy::ProxyKind::kCurl) {
    curl_proxy = proxy_spec.original_url;
  }

  CURL* curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_easy_init failed");

  struct curl_slist* hdrs = nullptr;
  for (const auto& [k, v] : headers) hdrs = curl_slist_append(hdrs, (k + ": " + v).c_str());

  std::string resp;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
  if (!curl_proxy.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, curl_proxy.c_str());
  if (insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  // KHÔNG set CURLOPT_ACCEPT_ENCODING -> nhận body thô (tự maybe_gunzip).

  CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) {
    std::string detail = ss_proxy ? ss_proxy->error() : "";
    throw std::runtime_error(std::string("curl: ") + curl_easy_strerror(rc) +
                             " (received=" + std::to_string(resp.size()) + " bytes)" +
                             (detail.empty() ? "" : " (Shadowsocks: " + detail + ")"));
  }
  return {status, resp};
}

std::pair<long, std::string> http_get(const std::string& url, const OrderedKV& headers,
                                      long timeout_sec, bool insecure,
                                      const std::string& proxy_url) {
  proxy::ProxySpec proxy_spec = proxy::parse_proxy_url(proxy_url);
  std::unique_ptr<proxy::LocalShadowsocksProxy> ss_proxy;
  std::string curl_proxy;
  if (proxy_spec.kind == proxy::ProxyKind::kShadowsocks) {
    ss_proxy = std::make_unique<proxy::LocalShadowsocksProxy>(proxy_spec, timeout_sec);
    curl_proxy = ss_proxy->curl_proxy_url();
  } else if (proxy_spec.kind == proxy::ProxyKind::kCurl) {
    curl_proxy = proxy_spec.original_url;
  }

  CURL* curl = curl_easy_init();
  if (!curl) throw std::runtime_error("curl_easy_init failed");
  struct curl_slist* hdrs = nullptr;
  for (const auto& [k, v] : headers)
    hdrs = curl_slist_append(hdrs, (k + ": " + v).c_str());

  std::string resp;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
  if (!curl_proxy.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, curl_proxy.c_str());
  if (insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  if (rc == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(hdrs);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK) {
    std::string detail = ss_proxy ? ss_proxy->error() : "";
    throw std::runtime_error(std::string("curl: ") + curl_easy_strerror(rc) +
                             " (received=" + std::to_string(resp.size()) + " bytes)" +
                             (detail.empty() ? "" : " (Shadowsocks: " + detail + ")"));
  }
  return {status, resp};
}

}  // namespace tencent::http
