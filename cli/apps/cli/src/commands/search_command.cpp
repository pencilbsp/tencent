// search_command.cpp — Lệnh `search`: tìm kiếm video ẩn danh (không cần account).
//
// Chỉ đổ tham số vào service::SearchRequest/SuggestRequest rồi in kết quả; toàn
// bộ nghiệp vụ nằm ở tencent::service::SearchService (tencent_core).
#include "../commands.h"
#include "../console.h"

#include <string>
#include <vector>

#include "tencent/core/service/search_service.h"

namespace tencent::cli {

namespace {

std::string join(const std::vector<std::string>& values, const std::string& separator) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out += separator;
    out += values[i];
  }
  return out;
}

// data_type 2 = cover (id là cid), 1 = video (id là vid).
std::string id_label(int data_type, const std::string& id) {
  if (data_type == 2) return "cid=" + id;
  if (data_type == 1) return "vid=" + id;
  return "id=" + id + " (type " + std::to_string(data_type) + ")";
}

int run_suggest(const SearchOptions& options) {
  const service::SearchService service;
  service::SuggestRequest request;
  request.query = options.query;
  request.page_size = options.page_size > 0 ? options.page_size : 10;
  request.timeout_sec = options.timeout_sec;
  request.proxy_url = options.proxy_url;

  auto result = service.suggest(request);
  if (!result) return report(result.error());

  if (result->empty()) {
    print("(không có gợi ý)");
    return 0;
  }
  for (const auto& item : result.value()) {
    if (item.text.empty()) continue;
    print("  " + item.text + (item.id.empty() ? "" : "   [" + item.id + "]"));
  }
  return 0;
}

}  // namespace

int run_search(const SearchOptions& options) {
  if (options.suggest) return run_suggest(options);

  const service::SearchService service;
  service::SearchRequest request;
  request.query = options.query;
  request.page = options.page;
  if (options.page_size > 0) request.page_size = options.page_size;
  request.filter_value = options.filter_value;
  request.pre_qid = options.pre_qid;
  request.timeout_sec = options.timeout_sec;
  request.proxy_url = options.proxy_url;

  auto result = service.search(request);
  if (!result) return report(result.error());

  const search::SearchPage& page = result.value();
  print("qid=" + page.qid + "  tổng=" + std::to_string(page.total_num) +
        "  trang=" + std::to_string(options.page) + "  hiển thị=" +
        std::to_string(page.items.size()));

  int index = 0;
  for (const auto& item : page.items) {
    std::string head = "[" + std::to_string(++index) + "] " + id_label(item.data_type, item.id);
    head += "  " + item.type_name;
    if (item.year > 0) head += " " + std::to_string(item.year);
    print(head + "  " + item.title);

    if (!item.badges.empty()) print("      " + join(item.badges, " · "));
    if (!item.actors.empty()) print("      " + join(item.actors, " "));
    if (options.show_episodes && !item.episodes.empty()) {
      std::vector<std::string> labels;
      for (const auto& episode : item.episodes) {
        labels.push_back(episode.title + (episode.marks.empty()
                                              ? ""
                                              : "(" + join(episode.marks, ",") + ")"));
      }
      print("      tập: " + join(labels, ", ") + "  (tổng " +
            std::to_string(item.total_episode) + ")");
    }
  }

  if (!page.items.empty() && !page.qid.empty()) {
    print("trang kế: tencent-cli search \"" + options.query + "\" --page " +
          std::to_string(options.page + 1) + " --pre-qid " + page.qid);
  }
  return 0;
}

}  // namespace tencent::cli
