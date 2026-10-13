// search_api.cpp — Xem include/tencent/core/protocol/search/search_api.h.
//
// Mọi hàm ở đây port từ ../analytics/app/search_web_client.py (xem từng hàm).
#include "tencent/core/protocol/search/search_api.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "tencent/core/common/json.h"

namespace tencent::search {

namespace {

using util::Json;
using util::OrderedJson;

constexpr int kClientTypeHippy = 1;               // search_web_client.py:CLIENT_TYPE_HIPPY
constexpr int kSuggestPlatVersion = 15;           // :SUG_PLAT_VERSION
constexpr const char* kSuggestAppId = "3168";     // :SUG_AUTH_INFO (hằng của bundle)
constexpr const char* kSuggestAppKey = "Ve3Z02Uwte4AH4tJ";

// Python json.dumps(ensure_ascii=False, separators=(",", ":")) == dump() mặc định
// của nlohmann (gọn, UTF-8 thô). `replace` để chuỗi UTF-8 hỏng không ném.
std::string dump_compact(const OrderedJson& value) {
  return value.dump(-1, ' ', false, OrderedJson::error_handler_t::replace);
}

// Python `str(v)` cho phần tử danh sách: chuỗi giữ nguyên, còn lại dump JSON.
std::vector<std::string> str_list(const Json& value) {
  std::vector<std::string> out;
  if (!value.is_array()) return out;
  for (const auto& element : value) {
    out.push_back(element.is_string() ? element.get<std::string>() : element.dump());
  }
  return out;
}

// Khoá sắp xếp của _mark_texts: int(key) nếu toàn chữ số, ngược lại "rất lớn".
uint64_t mark_sort_key(const std::string& key) {
  constexpr uint64_t kBig = uint64_t{1} << 30;
  if (key.empty() || key.size() > 9) return kBig;
  uint64_t value = 0;
  for (char c : key) {
    if (c < '0' || c > '9') return kBig;
    value = value * 10 + static_cast<uint64_t>(c - '0');
  }
  return value;
}

// Port search_web_client.py:_mark_texts — imgTag/markLabel là CHUỖI JSON
// {"1":{"info":{"text":..}},...}; lấy text theo thứ tự vị trí tăng dần, bỏ rỗng.
// Dùng ordered_json để giữ thứ tự chèn của các khoá không phải số (sort ổn định).
std::vector<std::string> mark_texts(const Json& raw) {
  std::vector<std::string> out;
  if (!raw.is_string() || raw.get<std::string>().empty()) return out;
  OrderedJson parsed = OrderedJson::parse(raw.get<std::string>(), nullptr, false);
  if (parsed.is_discarded() || !parsed.is_object()) return out;

  std::vector<std::pair<uint64_t, std::string>> found;
  for (auto it = parsed.begin(); it != parsed.end(); ++it) {
    if (!it.value().is_object()) continue;
    const auto info = it.value().find("info");
    if (info == it.value().end() || !info->is_object()) continue;
    const auto text = info->find("text");
    if (text == info->end() || !text->is_string() || text->get<std::string>().empty()) continue;
    found.emplace_back(mark_sort_key(it.key()), text->get<std::string>());
  }
  std::stable_sort(found.begin(), found.end(),
                   [](const auto& a, const auto& b) { return a.first < b.first; });
  for (auto& entry : found) out.push_back(std::move(entry.second));
  return out;
}

// Python `d.get(key) or {}`: thiếu/null/không-phải-object → object rỗng.
const Json& object_or_empty(const Json& parent, const char* key) {
  static const Json kEmpty = Json::object();
  if (!parent.is_object()) return kEmpty;
  const auto it = parent.find(key);
  if (it == parent.end() || !it->is_object()) return kEmpty;
  return *it;
}

const Json& array_or_empty(const Json& parent, const char* key) {
  static const Json kEmpty = Json::array();
  if (!parent.is_object()) return kEmpty;
  const auto it = parent.find(key);
  if (it == parent.end() || !it->is_array()) return kEmpty;
  return *it;
}

}  // namespace

std::string strip_tags(const std::string& text) {
  // Regex `<[^>]+>`: '<' + ít nhất một ký tự không phải '>' + '>'.
  std::string out;
  out.reserve(text.size());
  size_t i = 0;
  while (i < text.size()) {
    if (text[i] == '<') {
      const size_t close = text.find('>', i + 1);
      if (close != std::string::npos && close > i + 1) {
        i = close + 1;
        continue;
      }
    }
    out.push_back(text[i++]);
  }
  return out;
}

std::string build_search_body(const SearchQuery& query, const std::string& uuid) {
  OrderedJson body = OrderedJson::object();
  body["version"] = "";
  body["clientType"] = kClientTypeHippy;
  body["filterValue"] = query.filter_value;
  body["uuid"] = uuid;
  body["retry"] = 0;
  body["query"] = query.query;
  body["pagenum"] = query.page;
  body["pagesize"] = query.page_size;
  body["queryFrom"] = query.query_from;
  body["isneedQc"] = true;
  body["preQid"] = query.pre_qid;
  body["transInfo"] = query.trans_info;
  body["searchDatakey"] = query.search_datakey;
  OrderedJson extra = OrderedJson::object();
  extra["isNewMarkLabel"] = "0";
  extra["multi_terminal_pc"] = "0";
  extra["themeType"] = "0";
  body["extraInfo"] = std::move(extra);
  body["featureList"] = OrderedJson::array({"DEFAULT_FEFEATURE", "PC_SHORT_VIDEOS_WATERFALL"});
  return dump_compact(body);
}

std::string build_suggest_body(const SuggestQuery& query) {
  OrderedJson body = OrderedJson::object();
  OrderedJson auth = OrderedJson::object();
  auth["app_id"] = kSuggestAppId;
  auth["app_key"] = kSuggestAppKey;
  body["auth_info"] = std::move(auth);
  body["plat_version"] = kSuggestPlatVersion;
  body["version"] = "";
  body["page_num"] = 0;
  body["page_size"] = query.page_size;
  body["query"] = query.query;
  return dump_compact(body);
}

SearchPage parse_search_response(const std::string& response_body) {
  const Json root = Json::parse(response_body);
  if (!root.is_object()) throw std::runtime_error("phản hồi tìm kiếm không phải object");

  // search_web_client.py:search — ret != 0 hoặc data.errcode != 0 là lỗi.
  const int64_t ret = util::json_int(root, "ret", 0);
  if (ret != 0) {
    throw std::runtime_error("search ret=" + std::to_string(ret) + " msg=" +
                             util::json_string(root, "msg"));
  }
  const Json& data = object_or_empty(root, "data");
  const int64_t errcode = util::json_int(data, "errcode", 0);
  if (errcode != 0) {
    throw std::runtime_error("search errcode=" + std::to_string(errcode) + " " +
                             util::json_string(data, "errmsg"));
  }

  // search_web_client.py:parse_search_page
  const Json& normal = object_or_empty(data, "normalList");
  SearchPage page;
  page.qid = util::json_string(data, "qid");
  page.total_num = static_cast<int>(util::json_int(normal, "totalNum", 0));

  for (const Json& item : array_or_empty(normal, "itemList")) {
    const Json& doc = object_or_empty(item, "doc");
    const Json& info = object_or_empty(item, "videoInfo");
    if (info.empty()) continue;  // quảng cáo / box phụ không có videoInfo

    const Json& cover = object_or_empty(info, "coverDoc");
    const Json& sites = array_or_empty(info, "episodeSites");
    static const Json kNoSite = Json::object();
    const Json& site = (!sites.empty() && sites[0].is_object()) ? sites[0] : kNoSite;

    SearchItem out;
    out.data_type = static_cast<int>(util::json_int(doc, "dataType", 0));
    out.id = util::json_string(doc, "id");
    out.md = util::json_string(doc, "md");
    out.type_name = util::json_string(info, "typeName");
    out.title = strip_tags(util::json_string(info, "title"));
    out.sub_title = util::json_string(info, "subTitle");
    out.year = static_cast<int>(util::json_int(info, "year", 0));
    out.area = util::json_string(info, "area");
    out.languages = str_list(info.value("language", Json()));
    out.actors = str_list(info.value("actors", Json()));
    out.directors = str_list(info.value("directors", Json()));
    out.badges = mark_texts(info.value("imgTag", Json()));
    for (const Json& tag : array_or_empty(cover, "richTags")) {
      if (!tag.is_object()) continue;
      const std::string text = util::json_string(tag, "text");
      if (!text.empty()) out.tags.push_back(text);
    }
    out.total_episode = static_cast<int>(util::json_int(site, "totalEpisode", 0));
    for (const Json& ep : array_or_empty(site, "episodeInfoList")) {
      Episode episode;
      episode.id = util::json_string(ep, "id");
      episode.title = util::json_string(ep, "title");
      episode.url = util::json_string(ep, "url");
      episode.marks = mark_texts(ep.value("markLabel", Json()));
      out.episodes.push_back(std::move(episode));
    }
    page.items.push_back(std::move(out));
  }
  return page;
}

std::vector<SuggestItem> parse_suggest_response(const std::string& response_body) {
  const Json root = Json::parse(response_body);
  if (!root.is_object()) throw std::runtime_error("phản hồi gợi ý không phải object");

  // search_web_client.py:parse_suggest
  const Json& list = object_or_empty(object_or_empty(root, "data"), "result_list");
  std::vector<SuggestItem> out;
  for (const Json& item : array_or_empty(list, "item_list")) {
    const Json& doc = object_or_empty(item, "doc");
    const Json& view = object_or_empty(item, "view");

    SuggestItem entry;
    entry.data_type = static_cast<int>(util::json_int(doc, "data_type", 0));
    entry.id = util::json_string(doc, "id");
    entry.md = util::json_string(doc, "md");
    bool first_line = true;
    for (const Json& line : array_or_empty(view, "lines")) {
      if (!line.is_object()) continue;
      // " ".join(...): dấu cách đứng GIỮA các dòng, kể cả dòng rỗng.
      if (!first_line) entry.text += ' ';
      first_line = false;
      entry.text += strip_tags(util::json_string(line, "text"));
    }
    entry.image_url = util::json_string(view, "img_url");
    out.push_back(std::move(entry));
  }
  return out;
}

}  // namespace tencent::search
