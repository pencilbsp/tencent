// search_service.cpp — Xem include/tencent/core/service/search_service.h.
#include "tencent/core/service/search_service.h"

#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>

#include "tencent/core/net/http_client.h"
#include "tencent/core/service/error.h"

namespace tencent::service {

namespace {

// UA/headers giống trang tìm kiếm Hippy khi gọi fetch (search_web_client.py:_post).
constexpr const char* kUserAgent =
    "Mozilla/5.0 (Linux; Android 14; SM-G991N) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/131.0 Mobile Safari/537.36";

OrderedKV json_headers() {
  return {
      {"Content-Type", "application/json"},
      {"Accept", "application/json"},
      {"Referer", "https://v.qq.com"},
      {"Origin", "https://v.qq.com"},
      {"User-Agent", kUserAgent},
  };
}

// UUID v4 (8-4-4-4-12). Chỉ là id theo dõi request phía client, không bí mật.
std::string random_uuid() {
  std::random_device device;
  std::mt19937_64 engine((static_cast<uint64_t>(device()) << 32) ^ device());
  const uint64_t high = engine();
  const uint64_t low = engine();
  char buffer[40];
  std::snprintf(buffer, sizeof(buffer), "%08x-%04x-4%03x-%04x-%012llx",
                static_cast<unsigned>(high >> 32), static_cast<unsigned>((high >> 16) & 0xFFFF),
                static_cast<unsigned>(high & 0x0FFF),
                static_cast<unsigned>(0x8000 | ((low >> 48) & 0x3FFF)),
                static_cast<unsigned long long>(low & 0xFFFFFFFFFFFFULL));
  return buffer;
}

std::pair<long, std::string> default_transport(const std::string& url, const OrderedKV& headers,
                                               const std::string& body, long timeout_sec,
                                               const std::string& proxy_url) {
  auto response = http::http_post(url, headers, body, timeout_sec, /*insecure=*/false, proxy_url);
  response.second = http::maybe_gunzip(response.second);
  return response;
}

// Gửi một POST JSON; HTTP != 200 hoặc lỗi transport → kNetwork.
Result<std::string> post_json(const SearchServiceOptions& options, const char* path,
                              const std::string& body, long timeout_sec,
                              const std::string& proxy_url) {
  return guard<std::string>(ErrorCode::kNetwork, "không gọi được API tìm kiếm", [&] {
    const auto& transport = options.transport ? options.transport : default_transport;
    auto [status, response] =
        transport(std::string(search::kHost) + path, json_headers(), body, timeout_sec, proxy_url);
    if (status != 200) {
      throw std::runtime_error("HTTP " + std::to_string(status) + ": " + response.substr(0, 200));
    }
    return std::move(response);
  });
}

}  // namespace

Result<search::SearchPage> SearchService::search(const SearchRequest& request) const {
  if (request.query.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "thiếu từ khoá tìm kiếm");
  }
  if (request.page < 0 || request.page_size <= 0) {
    return make_error(ErrorCode::kInvalidArgument, "page/page_size không hợp lệ");
  }

  search::SearchQuery query;
  query.query = request.query;
  query.page = request.page;
  query.page_size = request.page_size;
  query.filter_value = request.filter_value;
  query.pre_qid = request.pre_qid;
  query.trans_info = request.trans_info;
  query.search_datakey = request.search_datakey;

  const std::string uuid = options_.uuid_source ? options_.uuid_source() : random_uuid();
  auto response = post_json(options_, search::kSearchPath, search::build_search_body(query, uuid),
                            request.timeout_sec, request.proxy_url);
  if (!response) return response.error();

  return guard<search::SearchPage>(ErrorCode::kProtocol, "phản hồi tìm kiếm không hợp lệ", [&] {
    return search::parse_search_response(response.value());
  });
}

Result<std::vector<search::SuggestItem>> SearchService::suggest(
    const SuggestRequest& request) const {
  if (request.query.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "thiếu từ khoá gợi ý");
  }
  if (request.page_size <= 0) {
    return make_error(ErrorCode::kInvalidArgument, "page_size không hợp lệ");
  }

  search::SuggestQuery query;
  query.query = request.query;
  query.page_size = request.page_size;

  auto response = post_json(options_, search::kSuggestPath, search::build_suggest_body(query),
                            request.timeout_sec, request.proxy_url);
  if (!response) return response.error();

  return guard<std::vector<search::SuggestItem>>(
      ErrorCode::kProtocol, "phản hồi gợi ý không hợp lệ",
      [&] { return search::parse_suggest_response(response.value()); });
}

}  // namespace tencent::service
