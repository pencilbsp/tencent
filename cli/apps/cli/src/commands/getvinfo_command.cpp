// getvinfo_command.cpp — Lệnh `getvinfo`: lấy thông tin/URL phát, kèm tuỳ chọn
// tải thẳng ra file.
//
// Nghiệp vụ nằm ở service::PlaybackService + service::DownloadService; file này
// lo phần dòng lệnh: tách id từ URL, ghi các file debug, in kết quả.
#include "../commands.h"
#include "../console.h"

#include <optional>

#include "tencent/core/common/strings.h"
#include "tencent/core/service/download_service.h"
#include "tencent/core/service/playback_service.h"
#include "tencent/platform/filesystem.h"

namespace tencent::cli {
namespace {

struct CoverIds {
  std::string cid;
  std::string vid;
};

// URL trang xem có dạng https://v.qq.com/x/cover/<cid>/<vid>.html
std::optional<CoverIds> parse_cover_url(const std::string& raw_url) {
  std::string url = util::trim(raw_url);
  if (const std::size_t query = url.find_first_of("?#"); query != std::string::npos) {
    url.resize(query);
  }
  while (!url.empty() && url.back() == '/') url.pop_back();

  const std::string marker = "/x/cover/";
  const std::size_t position = url.find(marker);
  if (position == std::string::npos) return std::nullopt;

  const std::vector<std::string> parts =
      util::split(url.substr(position + marker.size()), '/', /*keep_empty=*/false);
  if (parts.size() < 2) return std::nullopt;

  const auto strip_html = [](std::string value) {
    if (util::ends_with(value, ".html")) value.resize(value.size() - 5);
    return value;
  };
  CoverIds ids{strip_html(parts[0]), strip_html(parts[1])};
  if (ids.cid.empty() || ids.vid.empty()) return std::nullopt;
  return ids;
}

// Ghi file debug; trả false (đã in lỗi) nếu không ghi được.
bool write_debug_file(const std::string& path, const std::string& content,
                      const char* label) {
  if (path.empty()) return true;
  try {
    platform::write_file(path, content);
    print(std::string("  ") + label + "=" + path);
    return true;
  } catch (const std::exception& e) {
    print_error(std::string("không ghi được ") + label + ": " + e.what());
    return false;
  }
}

}  // namespace

bool resolve_ids_from_url(std::string& vid, std::string& cid, const std::string& url) {
  if (url.empty()) return !vid.empty();

  const auto ids = parse_cover_url(url);
  if (!ids) {
    print_error("--url không phải URL trang xem hợp lệ: " + url);
    print_error("ví dụ: https://v.qq.com/x/cover/<cid>/<vid>.html");
    return false;
  }
  if (!vid.empty() && vid != ids->vid) {
    print_error("--url cho vid=" + ids->vid + " nhưng --vid=" + vid);
    return false;
  }
  if (!cid.empty() && cid != ids->cid) {
    print_error("--url cho cid=" + ids->cid + " nhưng --cid=" + cid);
    return false;
  }
  vid = ids->vid;
  cid = ids->cid;
  return true;
}

int run_getvinfo(const GetvinfoOptions& options) {
  service::PlaybackRequest request;
  request.account_id = options.account_id;
  request.vid = options.vid;
  request.cid = options.cid;
  request.defn = options.defn;
  request.mode = options.mode;
  request.capabilities = options.capabilities;
  request.timeout_sec = options.timeout_sec;
  request.insecure = options.insecure;
  request.proxy_url = options.proxy_url;

  const service::PlaybackService playback;

  // ── Chỉ dựng request ──────────────────────────────────────────────────────
  if (options.dry_run) {
    auto built = playback.build(request);
    if (!built) return report(built.error());
    const service::PlaybackResult& result = built.value();
    print("  auth=account:" + result.account_vuid +
          (result.session_refreshed ? " (vừa làm mới)" : ""));
    print("# getvinfo vid=" + options.vid + " defn=" + options.defn +
          " mode=" + options.mode);
    print("  url=" + result.request.url);
    print("  params=" + std::to_string(result.request.params.size()) +
          " body_len=" + std::to_string(result.request.body.size()));
    if (!write_debug_file(options.request_out, result.request_text, "request_out")) return 1;
    print("");
    print(result.request.body);
    return 0;
  }

  // ── Gửi thật ──────────────────────────────────────────────────────────────
  auto fetched = playback.fetch(request);
  if (!fetched) return report(fetched.error());
  const service::PlaybackResult& result = fetched.value();

  print("  auth=account:" + result.account_vuid +
        (result.session_refreshed ? " (vừa làm mới)" : ""));
  print("# getvinfo vid=" + options.vid + " defn=" + options.defn + " mode=" + options.mode);
  print("  url=" + result.request.url);
  print("  params=" + std::to_string(result.request.params.size()) +
        " body_len=" + std::to_string(result.request.body.size()));

  if (!write_debug_file(options.request_out, result.request_text, "request_out")) return 1;
  if (!write_debug_file(options.response_out, result.response_text, "response_out")) return 1;
  if (result.anc_decoded) print("anc=đã giải mã");
  if (!write_debug_file(options.decoded_out, result.decoded.dump(2) + "\n", "decoded_out")) {
    return 1;
  }

  print("em=" + std::to_string(result.em) + " exem=" + std::to_string(result.exem) +
        (result.message.empty() ? "" : " msg=" + result.message));
  if (result.login >= 0) print("login=" + std::to_string(result.login));
  if (options.print_raw_json) print(result.decoded.dump(2));

  if (result.em != 0) {
    print_error(result.error_hint);
    return 1;
  }

  if (result.streams.empty()) {
    print("(em=0 nhưng không có vl.vi — dùng --raw để xem phản hồi đầy đủ)");
  }
  for (const auto& stream : result.streams) {
    print("# video: " + stream.title + "  fn=" + stream.file_name);
    for (std::size_t i = 0; i < stream.urls.size(); ++i) {
      print("  url[" + std::to_string(i) + "]: " + stream.urls[i]);
    }
  }

  if (options.output_path.empty()) return 0;

  // ── Tải luôn ──────────────────────────────────────────────────────────────
  const service::DownloadService downloader;
  auto capture = downloader.save_capture(options.download.work_dir, options.vid, options.defn,
                                         result.request_text, result.response_text);
  if (!capture) return report(capture.error());

  service::DownloadRequest download = options.download;
  download.request_path = capture.value().request_path;
  download.response_path = capture.value().response_path;
  download.output_path = options.output_path;
  download.vid = options.vid;
  download.defn = options.defn;
  download.timeout = options.timeout_sec;
  download.proxy_url = options.proxy_url;
  download.insecure = options.insecure;
  // User-Agent và ngữ cảnh appcfg phải khớp thiết bị của account vừa dùng.
  download.download_user_agent = result.download_user_agent;
  download.proxy_config_context = result.download_proxy_context;

  auto ran = downloader.run(download);
  if (!ran) return report(ran.error());
  return ran.value();
}

}  // namespace tencent::cli
