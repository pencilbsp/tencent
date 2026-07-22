// download_service.h — Nghiệp vụ TẢI VIDEO và quản lý cache.
//
// Bao gồm:
//   • lưu "capture" (request + response) vào cache theo vid/chất lượng, để lần
//     sau tải lại không cần gọi getvinfo;
//   • chạy pipeline tải HLS → giải mã DRM → remux MP4;
//   • dọn cache.
//
// Bản thân pipeline (download/pipeline.h) đã là lõi thuần; lớp này chỉ ghép nó
// với account/cache và biến ngoại lệ thành Result.
#pragma once

#include <cstdint>
#include <string>

#include "tencent/core/common/result.h"
#include "tencent/core/download/pipeline.h"
#include "tencent/core/service/account_service.h"
#include "tencent/core/service/playback_service.h"

namespace tencent::service {

// Tham số tải. Đây CHÍNH LÀ cấu hình mà pipeline nhận (download/pipeline.h) —
// đặt bí danh ở đây để giao diện chỉ phải nhắc tới `tencent::service::*`, và để
// đánh dấu rằng struct này thuộc bề mặt công khai của tầng service.
using DownloadRequest = download::Options;

// Nơi lưu capture của một lần lấy thông tin phát.
struct CaptureLocation {
  std::string cache_dir;      // <work>/videos/<vid>/<defn>
  std::string request_path;   // …/capture/request.txt
  std::string response_path;  // …/capture/response.json
};

struct FlushOutcome {
  std::string target;      // thư mục đã (hoặc sẽ) xoá
  uint64_t bytes = 0;      // dung lượng trước khi xoá
  uint64_t entries = 0;    // số mục đã xoá (0 khi dry-run)
  bool existed = true;
  bool dry_run = false;
};

class DownloadService {
 public:
  explicit DownloadService(AccountServiceOptions options = AccountServiceOptions())
      : accounts_(options) {}

  // Ghi request/response vào cache và trả về đường dẫn đã dùng.
  Result<CaptureLocation> save_capture(const std::string& work_dir, const std::string& vid,
                                       const std::string& defn,
                                       const std::string& request_text,
                                       const std::string& response_text) const;

  // Chạy pipeline. Trả về mã kết thúc của pipeline (0 = thành công).
  Result<int> run(const download::Options& options) const;

  // Điền `download_user_agent` từ account nếu người dùng không tự truyền.
  // Cần cho lệnh `download` độc lập (không đi qua getvinfo).
  Status apply_account_defaults(download::Options& options,
                                const std::string& account_id) const;

  // Xoá cache: toàn bộ, theo vid, hoặc theo vid+chất lượng.
  Result<FlushOutcome> flush(const std::string& work_dir, const std::string& vid,
                             const std::string& defn, bool dry_run) const;

 private:
  AccountService accounts_;
};

}  // namespace tencent::service
