// search_api.h — Dựng body và parse phản hồi cho TÌM KIẾM ẨN DANH.
//
// Màn tìm kiếm của app là bundle Hippy (JS) gọi trực tiếp HTTP+JSON tới
// pbaccess.video.qq.com — không cần đăng nhập/cookie. Hai API:
//   • kết quả : POST kSearchPath  (MbSearchHttp)
//   • gợi ý   : POST kSuggestPath (GetSugHttp)
//
// Nguồn (ground-truth Python): ../analytics/app/search_web_client.py
//   build_search_body / build_suggest_body / parse_search_page / parse_suggest.
// Phân tích: bundle hippy_search_android_3 (APK assets/res_hub/...), đã giải nén
// ở ../analytics/app/hippy_search/bundle/index.android.jsbundle.
//
// Mô-đun này THUẦN: không mạng, không đọc đồng hồ. uuid do người gọi truyền vào
// để body tất định (test byte-exact: tests/protocol/test_search_api.cpp).
#pragma once

#include <string>
#include <vector>

namespace tencent::search {

// Host + đường dẫn trpc (search_web_client.py: HOST/PATH_SEARCH/PATH_SUGGEST).
constexpr const char* kHost = "https://pbaccess.video.qq.com";
constexpr const char* kSearchPath =
    "/trpc.videosearch.mobile_search.HttpMobileRecall/MbSearchHttp";
constexpr const char* kSuggestPath =
    "/trpc.videosearch.smartboxServer.SugRecallHttp/GetSugHttp";

// ── Yêu cầu ─────────────────────────────────────────────────────────────────
struct SearchQuery {
  std::string query;
  int page = 0;               // pagenum, bắt đầu từ 0
  int page_size = 15;         // pagesize
  std::string filter_value;   // giá trị tab lọc (xem filters trong phản hồi)
  std::string pre_qid;        // `qid` của trang trước khi lật trang
  std::string trans_info;
  std::string search_datakey;
  int query_from = 0;         // 0 = gõ tay (enum queryFrom trong bundle)
};

struct SuggestQuery {
  std::string query;
  int page_size = 10;
};

// Body JSON gọn (không dấu cách) đúng thứ tự khoá của bundle.
// Port search_web_client.py:build_search_body + dump_body.
std::string build_search_body(const SearchQuery& query, const std::string& uuid);

// Port search_web_client.py:build_suggest_body + dump_body.
// auth_info.app_id/app_key là hằng của client công khai trong bundle, KHÔNG phải
// thông tin đăng nhập của người dùng.
std::string build_suggest_body(const SuggestQuery& query);

// ── Kết quả ─────────────────────────────────────────────────────────────────
struct Episode {
  std::string id;
  std::string title;
  std::string url;
  std::vector<std::string> marks;   // nhãn trên ô tập: "加更礼", "预告", ...
};

struct SearchItem {
  int data_type = 0;   // 2 = cover (id là cid "mzc..."), 1 = video (id là vid)
  std::string id;
  std::string md;
  std::string type_name;           // 电视剧 / 动漫 / 综艺 ...
  std::string title;               // đã bỏ thẻ <em>
  std::string sub_title;
  int year = 0;
  std::string area;
  std::vector<std::string> languages;
  std::vector<std::string> actors;
  std::vector<std::string> directors;
  std::vector<std::string> badges;  // imgTag theo thứ tự: "2020", "独播", "更新至648集"
  std::vector<std::string> tags;    // coverDoc.richTags[].text
  int total_episode = 0;            // episodeSites[0].totalEpisode
  std::vector<Episode> episodes;    // episodeSites[0].episodeInfoList
};

struct SearchPage {
  std::string qid;       // dùng làm pre_qid cho trang kế tiếp
  int total_num = 0;
  std::vector<SearchItem> items;    // chỉ mục có videoInfo (bỏ quảng cáo/box phụ)
};

struct SuggestItem {
  int data_type = 0;     // 0 = gợi ý chữ; thẻ series có data_type khác
  std::string id;
  std::string md;
  std::string text;      // các dòng view.lines[].text nối bằng ' ', đã bỏ thẻ
  std::string image_url;
};

// Ném std::runtime_error nếu `ret`/`errcode` != 0; std::exception nếu JSON hỏng.
// Port search_web_client.py:search (kiểm ret/errcode) + parse_search_page.
SearchPage parse_search_response(const std::string& response_body);

// Port search_web_client.py:parse_suggest.
std::vector<SuggestItem> parse_suggest_response(const std::string& response_body);

// Bỏ thẻ kiểu <em>..</em> (regex gốc `<[^>]+>`). Lộ ra để test.
std::string strip_tags(const std::string& text);

}  // namespace tencent::search
