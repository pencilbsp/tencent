// download_service.cpp — Xem include/tencent/core/service/download_service.h.
//
// Chuyển từ cmd_download() / cmd_flush() và nhánh `-o` của cmd_getvinfo()
// trong tencent_cli_main.cpp cũ.
#include "tencent/core/service/download_service.h"

#include <filesystem>

#include "tencent/core/service/error.h"
#include "tencent/core/storage/paths.h"
#include "tencent/platform/filesystem.h"

namespace tencent::service {
namespace fs = std::filesystem;

Result<CaptureLocation> DownloadService::save_capture(const std::string& work_dir,
                                                      const std::string& vid,
                                                      const std::string& defn,
                                                      const std::string& request_text,
                                                      const std::string& response_text) const {
  return guard<CaptureLocation>(ErrorCode::kIo, "không ghi được capture để tải", [&] {
    const std::string root = work_dir.empty() ? download::default_work_dir() : work_dir;
    CaptureLocation location;
    location.cache_dir = download::cache_work_dir(root, vid, defn);
    const fs::path capture = fs::path(location.cache_dir) / "capture";
    location.request_path = (capture / "request.txt").string();
    location.response_path = (capture / "response.json").string();
    platform::write_file(location.request_path, request_text);
    platform::write_file(location.response_path, response_text);
    return location;
  });
}

Result<int> DownloadService::run(const download::Options& options) const {
  return guard<int>(ErrorCode::kInternal, "tải thất bại",
                    [&] { return download::run_download(options); });
}

Status DownloadService::apply_account_defaults(download::Options& options,
                                               const std::string& account_id) const {
  // User-Agent của DownloadProxy đi theo phiên bản native trên MÁY người dùng;
  // sai giá trị này thì CDN có thể trả 403.
  if (!options.download_user_agent.empty() || options.no_download) return Status();

  auto resolved = accounts_.resolve_account_id(account_id);
  if (!resolved) return Status();   // không có account nào: cứ để trống như cũ

  auto loaded = accounts_.load(resolved.value());
  if (!loaded) return loaded.error();
  options.download_user_agent = loaded.value().download_proxy_version;
  options.proxy_config_context.version = options.download_user_agent;
  options.proxy_config_context.guid = loaded.value().guid;
  options.proxy_config_context.qimei36 = loaded.value().qimei36;
  options.proxy_config_context.device_model = loaded.value().model;
  return Status();
}

Result<FlushOutcome> DownloadService::flush(const std::string& work_dir, const std::string& vid,
                                            const std::string& defn, bool dry_run) const {
  if (!defn.empty() && vid.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "--defn phải đi kèm --vid");
  }
  return guard<FlushOutcome>(ErrorCode::kIo, "không dọn được cache", [&] {
    const fs::path root =
        work_dir.empty() ? fs::path(download::default_work_dir()) : fs::path(work_dir);

    fs::path target;
    if (vid.empty()) {
      target = root / "videos";                      // toàn bộ cache video
    } else if (defn.empty()) {
      // Mọi chất lượng của một vid = thư mục cha của một mục chất lượng bất kỳ.
      target = fs::path(download::cache_work_dir(root.string(), vid, "_")).parent_path();
    } else {
      target = download::cache_work_dir(root.string(), vid, defn);
    }

    std::error_code ec;
    target = fs::absolute(target, ec);
    if (ec) ec.clear();

    FlushOutcome outcome;
    outcome.target = target.string();
    outcome.dry_run = dry_run;
    if (!fs::exists(target, ec)) {
      outcome.existed = false;
      return outcome;
    }
    outcome.bytes = platform::directory_size_bytes(outcome.target);
    if (dry_run) return outcome;
    outcome.entries = platform::remove_all(outcome.target);
    return outcome;
  });
}

}  // namespace tencent::service
