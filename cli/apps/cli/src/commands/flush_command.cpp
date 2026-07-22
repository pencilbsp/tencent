// flush_command.cpp — Lệnh `flush`: dọn cache video.
#include "../commands.h"
#include "../console.h"

#include "tencent/core/service/download_service.h"

namespace tencent::cli {

int run_flush(const FlushOptions& options) {
  const service::DownloadService service;
  auto result = service.flush(options.work_dir, options.vid, options.defn, options.dry_run);
  if (!result) return report(result.error());

  const service::FlushOutcome& outcome = result.value();
  if (!outcome.existed) {
    print("flush: không có cache tại " + outcome.target);
    return 0;
  }
  if (outcome.dry_run) {
    print("flush (thử): sẽ xoá " + outcome.target + " (" + format_bytes(outcome.bytes) + ")");
    return 0;
  }
  print("flush: đã xoá " + outcome.target + " (" + format_bytes(outcome.bytes) + ", " +
        std::to_string(outcome.entries) + " mục)");
  return 0;
}

}  // namespace tencent::cli
