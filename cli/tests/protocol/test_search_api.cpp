// test_search_api.cpp — BYTE-EXACT cho protocol/search + service/search_service.
//
// Ground-truth: tests/vectors/gen_vectors_search.py (import
// ../analytics/app/search_web_client.py) → generated_vectors_search.h.
// Fixture: tests/fixtures/search*_sample.json = response CAPTURE THẬT đã rút gọn.
// Chạy từ thư mục tests/ (xem tests/CMakeLists.txt) nên fixture đọc theo đường
// dẫn tương đối.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tencent/core/protocol/search/search_api.h"
#include "tencent/core/service/search_service.h"
#include "vectors/generated_vectors_search.h"

namespace search = tencent::search;
namespace service = tencent::service;

static int failures = 0;

static void check(bool ok, const std::string& name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) ++failures;
}

static std::string slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

static std::string join(const std::vector<std::string>& values, const char* sep) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out += sep;
    out += values[i];
  }
  return out;
}

// Phải khớp từng ký tự với render_page() trong gen_vectors_search.py.
static std::string render_page(const search::SearchPage& page) {
  std::string out = "qid=" + page.qid + "\ntotal=" + std::to_string(page.total_num) +
                    "\nitems=" + std::to_string(page.items.size()) + "\n";
  for (const auto& it : page.items) {
    out += "item|" + std::to_string(it.data_type) + "|" + it.id + "|" + it.md + "|" +
           it.type_name + "|" + it.title + "|" + it.sub_title + "|" + std::to_string(it.year) +
           "|" + it.area + "|" + join(it.languages, ",") + "|" + join(it.actors, ",") + "|" +
           join(it.directors, ",") + "|" + join(it.badges, ",") + "|" + join(it.tags, ",") + "|" +
           std::to_string(it.total_episode) + "|" + std::to_string(it.episodes.size()) + "\n";
    for (const auto& ep : it.episodes) {
      out += "ep|" + ep.id + "|" + ep.title + "|" + ep.url + "|" + join(ep.marks, ";") + "\n";
    }
  }
  return out;
}

static std::string render_suggest(const std::vector<search::SuggestItem>& items) {
  std::string out = "count=" + std::to_string(items.size()) + "\n";
  for (const auto& it : items) {
    out += "sug|" + std::to_string(it.data_type) + "|" + it.id + "|" + it.md + "|" + it.text +
           "|" + it.image_url + "\n";
  }
  return out;
}

int main() {
  std::printf("== search ==\n");

  // ── body request (byte-exact vs Python) ───────────────────────────────────
  for (const auto& c : vsearch::kSearchBodies) {
    search::SearchQuery q;
    q.query = c.query;
    q.page = c.page;
    q.page_size = c.page_size;
    q.filter_value = c.filter_value;
    q.pre_qid = c.pre_qid;
    q.trans_info = c.trans_info;
    q.search_datakey = c.search_datakey;
    q.query_from = c.query_from;
    check(search::build_search_body(q, vsearch::kUuid) == c.expected,
          std::string("search body: ") + c.name);
  }
  for (const auto& c : vsearch::kSuggestBodies) {
    search::SuggestQuery q;
    q.query = c.query;
    q.page_size = c.page_size;
    check(search::build_suggest_body(q) == c.expected, std::string("suggest body: ") + c.name);
  }

  // ── parse response CAPTURE THẬT (byte-exact vs Python parse_*) ────────────
  const std::string search_json = slurp("fixtures/search_response.sample.json");
  const std::string suggest_json = slurp("fixtures/search_suggest_response.sample.json");
  check(!search_json.empty() && !suggest_json.empty(), "fixtures đọc được");

  const search::SearchPage page = search::parse_search_response(search_json);
  check(render_page(page) == vsearch::kSearchPageExpected, "parse search page == Python");
  check(page.items.size() == 3, "bỏ item không có videoInfo (quảng cáo/rec_query)");
  check(!page.items.empty() && page.items[0].id == "mzc00200ilydv1a" &&
            page.items[0].data_type == 2,
        "item đầu là cover mzc00200ilydv1a (dataType 2)");

  const auto suggest = search::parse_suggest_response(suggest_json);
  check(render_suggest(suggest) == vsearch::kSuggestExpected, "parse suggest == Python");

  // ── strip_tags khớp regex <[^>]+> (expected do Python sw.plain() sinh) ─────
  for (const auto& c : vsearch::kStripCases) {
    check(search::strip_tags(c.input) == c.expected, std::string("strip_tags: '") + c.input + "'");
  }

  // ── lỗi từ server ─────────────────────────────────────────────────────────
  bool threw = false;
  try {
    search::parse_search_response(R"({"ret":-1,"msg":"bad","data":{}})");
  } catch (const std::exception&) {
    threw = true;
  }
  check(threw, "ret != 0 ném lỗi");
  threw = false;
  try {
    search::parse_search_response(R"({"ret":0,"data":{"errcode":5,"errmsg":"x"}})");
  } catch (const std::exception&) {
    threw = true;
  }
  check(threw, "errcode != 0 ném lỗi");

  // ── service với transport giả (không mạng) ────────────────────────────────
  std::string seen_url, seen_body;
  tencent::OrderedKV seen_headers;
  long next_status = 200;
  std::string next_body = search_json;

  service::SearchServiceOptions options;
  options.uuid_source = [] { return std::string(vsearch::kUuid); };
  options.transport = [&](const std::string& url, const tencent::OrderedKV& headers,
                          const std::string& body, long, const std::string&) {
    seen_url = url;
    seen_headers = headers;
    seen_body = body;
    return std::make_pair(next_status, next_body);
  };
  const service::SearchService svc(options);

  service::SearchRequest req;
  req.query = "无上神帝";
  auto result = svc.search(req);
  check(result.ok() && result->items.size() == 3, "service.search parse từ transport giả");
  check(seen_url == "https://pbaccess.video.qq.com"
                    "/trpc.videosearch.mobile_search.HttpMobileRecall/MbSearchHttp",
        "service.search URL");
  check(seen_body == vsearch::kSearchBodies[0].expected, "service.search body == vector 'simple'");
  check(seen_headers.size() == 5 && seen_headers[0].first == "Content-Type" &&
            seen_headers[3].first == "Origin",
        "service.search header giữ thứ tự");

  service::SuggestRequest sreq;
  sreq.query = "无上神帝";
  next_body = suggest_json;
  auto sresult = svc.suggest(sreq);
  check(sresult.ok() && sresult->size() == 4, "service.suggest parse từ transport giả");
  check(seen_url == "https://pbaccess.video.qq.com"
                    "/trpc.videosearch.smartboxServer.SugRecallHttp/GetSugHttp",
        "service.suggest URL");
  check(seen_body == vsearch::kSuggestBodies[0].expected, "service.suggest body == vector 'simple'");

  next_status = 500;
  next_body = "oops";
  auto http_error = svc.search(req);
  check(!http_error.ok() && http_error.code() == tencent::ErrorCode::kNetwork,
        "HTTP 500 → kNetwork");

  next_status = 200;
  next_body = "not json";
  auto bad_json = svc.search(req);
  check(!bad_json.ok() && bad_json.code() == tencent::ErrorCode::kProtocol,
        "JSON hỏng → kProtocol");

  next_body = R"({"ret":0,"data":{"errcode":7,"errmsg":"limit"}})";
  auto server_error = svc.search(req);
  check(!server_error.ok() && server_error.code() == tencent::ErrorCode::kProtocol,
        "errcode != 0 → kProtocol");

  service::SearchRequest empty;
  auto invalid = svc.search(empty);
  check(!invalid.ok() && invalid.code() == tencent::ErrorCode::kInvalidArgument,
        "từ khoá rỗng → kInvalidArgument");

  std::printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILED", failures);
  return failures == 0 ? 0 : 1;
}
