// search_service.h — Nghiệp vụ TÌM KIẾM video (ẩn danh, không cần account).
//
// Hai thao tác:
//   • search()  : trang kết quả (MbSearchHttp) — thẻ series/video + dải tập.
//   • suggest() : gợi ý khi gõ (GetSugHttp).
// Cả hai đều là HTTP+JSON tới pbaccess.video.qq.com y như trang tìm kiếm Hippy
// của app khi CHƯA đăng nhập, nên không đụng tới AccountService/vusession.
//
// Lỗi transport → ErrorCode::kNetwork; server trả ret/errcode != 0 hoặc JSON
// hỏng → ErrorCode::kProtocol. Không ngoại lệ nào thoát ra khỏi lớp này.
#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/common/result.h"
#include "tencent/core/protocol/search/search_api.h"

namespace tencent::service {

struct SearchRequest {
  std::string query;           // bắt buộc, không rỗng
  int page = 0;                // trang bắt đầu từ 0
  int page_size = 15;
  std::string filter_value;    // tab lọc (filters.firstLine[].paramVal)
  std::string pre_qid;         // qid của trang trước — cần khi lật trang
  std::string trans_info;
  std::string search_datakey;
  long timeout_sec = 20;
  std::string proxy_url;
};

struct SuggestRequest {
  std::string query;           // bắt buộc, không rỗng
  int page_size = 10;
  long timeout_sec = 20;
  std::string proxy_url;
};

// Điểm tiêm để test/GUI thay transport hoặc nguồn uuid mà không đụng mạng.
struct SearchServiceOptions {
  // (url, headers, body, timeout_sec, proxy_url) -> (http_status, thân thô).
  // Mặc định: http::http_post + http::maybe_gunzip.
  using Transport = std::function<std::pair<long, std::string>(
      const std::string&, const OrderedKV&, const std::string&, long, const std::string&)>;
  Transport transport;
  std::function<std::string()> uuid_source;   // mặc định: UUID v4 ngẫu nhiên
};

class SearchService {
 public:
  explicit SearchService(SearchServiceOptions options = SearchServiceOptions())
      : options_(std::move(options)) {}

  Result<search::SearchPage> search(const SearchRequest& request) const;
  Result<std::vector<search::SuggestItem>> suggest(const SuggestRequest& request) const;

 private:
  SearchServiceOptions options_;
};

}  // namespace tencent::service
