// playback_service.cpp — Xem include/tencent/core/service/playback_service.h.
//
// Chuyển từ cmd_getvinfo() trong tencent_cli_main.cpp cũ. Khác biệt: không in,
// không ghi file, không trả mã thoát — chỉ trả PlaybackResult hoặc Error.
// Việc lưu capture/tải video do DownloadService đảm nhiệm.
#include "tencent/core/service/playback_service.h"

#include <sstream>

#include "tencent/core/common/clock.h"
#include "tencent/core/download/pipeline.h"
#include "tencent/core/net/http_client.h"
#include "tencent/core/protocol/playback/request_context.h"
#include "tencent/core/service/error.h"
#include "tencent/core/storage/account_store.h"

namespace tencent::service {
namespace {

// Diễn giải mã lỗi getvinfo thành câu người dùng hiểu được.
// Nguồn: ../analytics/app/plan_getvinfo_params.md §394 (em=85) và quan sát thực tế.
std::string explain_error(int em, int exem, const util::Json& decoded) {
  if (em == 0) return "";
  if (em == 85 && exem == -3) {
    return "máy chủ từ chối cKey (85.-3) — thường do hằng số theo bản dựng đã cũ "
           "hoặc phiên đăng nhập không còn hợp lệ";
  }
  if (em == 80 && exem == 1) {
    return "nội dung bị giới hạn theo vùng (80.1): " +
           decoded.value("exinfo", std::string("không rõ vùng"));
  }
  return "getvinfo trả lỗi em=" + std::to_string(em) + " exem=" + std::to_string(exem) +
         " — kiểm tra lại phiên đăng nhập và tham số request";
}

// Ghép URL phát cuối cùng từ base + tên file + vkey, đúng như native làm.
std::vector<PlaybackStream> extract_streams(const util::Json& decoded) {
  std::vector<PlaybackStream> streams;
  if (!decoded.contains("vl") || !decoded["vl"].contains("vi") ||
      !decoded["vl"]["vi"].is_array()) {
    return streams;
  }
  for (const auto& video : decoded["vl"]["vi"]) {
    PlaybackStream stream;
    stream.title = video.value("ti", std::string());
    stream.file_name = video.value("fn", std::string());
    stream.vkey = video.value("fvkey", std::string());
    if (video.contains("ul") && video["ul"].contains("ui") && video["ul"]["ui"].is_array()) {
      for (const auto& entry : video["ul"]["ui"]) {
        const std::string base = entry.value("url", std::string());
        if (base.empty()) continue;
        // URL đã trỏ thẳng tới file thì dùng nguyên; ngược lại phải nối tên file
        // và vkey (server trả base thư mục).
        const bool complete = base.find(".m3u8") != std::string::npos ||
                              base.find(".mp4") != std::string::npos ||
                              base.find(".ts") != std::string::npos;
        std::string url = complete ? base : base + stream.file_name;
        if (!complete && !stream.vkey.empty()) url += "?vkey=" + stream.vkey;
        stream.urls.push_back(std::move(url));
      }
    }
    streams.push_back(std::move(stream));
  }
  return streams;
}

}  // namespace

std::string PlaybackService::serialize(const playback::FullRequest& request) {
  std::ostringstream out;
  out << request.method << ' ' << request.url << '\n';
  for (const auto& [name, value] : request.headers) out << name << ": " << value << '\n';
  out << '\n' << request.body << '\n';
  return out.str();
}

Result<PlaybackResult> PlaybackService::build(const PlaybackRequest& request) const {
  if (request.vid.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "thiếu --vid (hoặc --url)");
  }

  auto account_id = accounts_.resolve_account_id(request.account_id);
  if (!account_id) return account_id.error();
  auto loaded = accounts_.load(account_id.value());
  if (!loaded) return loaded.error();

  PlaybackResult result;
  account::Account account = loaded.value();
  if (const std::string missing = account::missing_required(account); !missing.empty()) {
    return make_error(ErrorCode::kInvalidArgument,
                      "account " + account_id.value() + " thiếu trường bắt buộc", missing);
  }

  // Bảo đảm vusession còn hạn TRƯỚC khi dựng cookie (cookie chứa vusession).
  if (request.allow_refresh) {
    auto refreshed = accounts_.ensure_fresh_session(account, accounts_.path_for(account_id.value()));
    if (!refreshed) return refreshed.error();
    result.session_refreshed = refreshed.value();
  }

  result.account_vuid = account.vuid;
  result.download_user_agent = account.download_proxy_version;
  result.download_proxy_context.version = account.download_proxy_version;
  result.download_proxy_context.guid = account.guid;
  result.download_proxy_context.qimei36 = account.qimei36;
  result.download_proxy_context.device_model = account.model;

  auto built = guard<playback::FullRequest>(
      ErrorCode::kInvalidArgument, "không dựng được request getvinfo", [&] {
        const playback::GetvinfoContext context =
            playback::build_request_context(account, static_cast<int64_t>(util::now_millis()));
        playback::BuildOptions options;
        options.vid = request.vid;
        options.defn = request.defn;
        options.mode = request.mode;
        if (!request.cid.empty()) options.cid = request.cid;
        options.extra = playback::build_capability_overrides(context, request.capabilities);
        return playback::build_full_request(context, options);
      });
  if (!built) return built.error();

  result.request = built.value();
  result.request_text = serialize(result.request);
  return result;
}

Result<PlaybackResult> PlaybackService::fetch(const PlaybackRequest& request) const {
  auto prepared = build(request);
  if (!prepared) return prepared.error();
  PlaybackResult result = prepared.value();

  auto response = guard<std::string>(ErrorCode::kNetwork, "gọi getvinfo thất bại", [&] {
    const auto [status, raw] =
        http::http_post(result.request.url, result.request.headers, result.request.body,
                        request.timeout_sec, request.insecure, request.proxy_url);
    if (status != 200) throw std::runtime_error("HTTP " + std::to_string(status));
    return http::maybe_gunzip(raw);
  });
  if (!response) {
    // Gợi ý thường gặp: vv6.video.qq.com có chuỗi chứng chỉ mà curl không tin.
    return make_error(response.error().code, response.error().message,
                      response.error().detail +
                          (request.insecure ? "" : " (nếu là lỗi TLS, thử --insecure)"));
  }
  result.response_text = response.value();

  auto parsed = guard<util::Json>(ErrorCode::kProtocol, "phản hồi getvinfo không phải JSON",
                                  [&] { return util::Json::parse(result.response_text); });
  if (!parsed) return parsed.error();
  result.decoded = parsed.value();

  // Phong bì ANC: thân thật nằm trong trường mã hoá, giải bằng chính cKey của
  // request vừa gửi.
  const bool has_anc =
      result.decoded.contains("anc") ||
      (result.decoded.contains("resData") && result.decoded["resData"].is_object() &&
       result.decoded["resData"].contains("anc"));
  if (has_anc) {
    auto decrypted = guard<util::Json>(ErrorCode::kProtocol, "không giải được phong bì ANC", [&] {
      return download::decrypt_anc(result.response_text, result.request_text);
    });
    if (!decrypted) return decrypted.error();
    result.decoded = decrypted.value();
    result.anc_decoded = true;
  }

  result.em = result.decoded.value("em", -1);
  result.exem = result.decoded.value("exem", 0);
  result.message = result.decoded.value("msg", std::string());
  if (result.decoded.contains("login")) result.login = result.decoded.value("login", 0);
  result.error_hint = explain_error(result.em, result.exem, result.decoded);
  if (result.em == 0) result.streams = extract_streams(result.decoded);
  return result;
}

}  // namespace tencent::service
