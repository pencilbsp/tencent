// http_client.h — Transport dùng chung: gzip (zlib) + HTTP POST (libcurl).
//
// Port ../analytics/app/login_refresh_transport.py (post_refresh/_maybe_gunzip/build_headers).
// KHÔNG byte-exact (mạng + gzip không tất định) — chỉ test roundtrip/smoke; tính
// đúng được verify live. Dùng chung cho cả getvinfo (Pha 6) và vipinfo.
#pragma once

#include <string>
#include <utility>

#include "tencent/core/common/ordered_kv.h"

namespace tencent::http {

// gzip nén/giải (zlib, windowBits 15+16). gzip_compress dùng level mặc định —
// server giải nén được nên KHÔNG cần khớp byte với Python.
std::string gzip_compress(const std::string& data);
std::string gunzip(const std::string& data);
// Giải nén nếu có magic 1f 8b, ngược lại trả nguyên (../analytics/app/...:68-74).
std::string maybe_gunzip(const std::string& data);

// POST body với header GIỮ THỨ TỰ. Trả (http_status, raw_response_bytes).
// Ném std::runtime_error nếu lỗi transport. Không tự giải nén (gọi maybe_gunzip).
// insecure=true: bỏ verify TLS (CURLOPT_SSL_VERIFYPEER/HOST=0) — cho host có
// chain lạ như vv6.video.qq.com (khớp --insecure của getvinfo_client.py).
std::pair<long, std::string> http_post(const std::string& url, const OrderedKV& headers,
                                       const std::string& body, long timeout_sec = 20,
                                       bool insecure = false,
                                       const std::string& proxy_url = "");

std::pair<long, std::string> http_get(const std::string& url, const OrderedKV& headers = {},
                                      long timeout_sec = 20, bool insecure = false,
                                      const std::string& proxy_url = "");

}  // namespace tencent::http
