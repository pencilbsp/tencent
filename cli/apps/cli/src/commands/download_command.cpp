// download_command.cpp — Lệnh `download`: tải/giải mã từ cặp request+response
// đã lưu trước đó (không cần gọi lại getvinfo).
#include "../commands.h"
#include "../console.h"

#include "tencent/core/service/download_service.h"

namespace tencent::cli {

int run_download(const DownloadOptions& options) {
  const service::DownloadService service;

  service::DownloadRequest download = options.download;
  // Nếu người dùng không tự truyền --user-agent thì lấy theo account (phiên bản
  // DownloadProxy native phải khớp, nếu không CDN có thể từ chối).
  if (const Status status = service.apply_account_defaults(download, options.account_id);
      !status) {
    return report(status.error());
  }

  auto result = service.run(download);
  if (!result) return report(result.error());
  return result.value();
}

}  // namespace tencent::cli
