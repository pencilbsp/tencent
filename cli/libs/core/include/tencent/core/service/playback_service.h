// playback_service.h — Nghiệp vụ PHÁT LẠI: lấy thông tin phát (getvinfo).
//
// Một lời gọi `fetch()` gói trọn chuỗi việc mà trước đây CLI phải tự xâu chuỗi:
//   chọn account → bảo đảm vusession còn hạn → dựng ngữ cảnh từ hằng số +
//   account → dựng body (đã test byte-exact) → POST → giải gzip → giải phong bì
//   ANC nếu có → đọc em/exem + danh sách luồng.
//
// Dịch vụ KHÔNG coi `em != 0` là lỗi transport: kết quả vẫn trả về đầy đủ (kèm
// `error_hint` đã diễn giải sẵn) để giao diện tự quyết cách hiển thị.
#pragma once

#include <string>
#include <vector>

#include "tencent/core/common/json.h"
#include "tencent/core/common/result.h"
#include "tencent/core/download/proxy_config.h"
#include "tencent/core/protocol/playback/capability.h"
#include "tencent/core/protocol/playback/request_builder.h"
#include "tencent/core/service/account_service.h"

namespace tencent::service {

struct PlaybackRequest {
  std::string account_id;                 // rỗng = account mặc định
  std::string vid;                        // bắt buộc
  std::string cid;
  std::string defn = "uhd";               // chất lượng NGƯỜI DÙNG chọn
  std::string mode = "normal";            // "normal" | "history"
  playback::CapabilityOptions capabilities;

  long timeout_sec = 20;
  bool insecure = false;                  // vv6.video.qq.com dùng chain lạ
  std::string proxy_url;
  bool allow_refresh = true;              // cho phép tự làm mới vusession
};

// Một luồng video trong phản hồi (vl.vi[]).
struct PlaybackStream {
  std::string title;        // vi.ti
  std::string file_name;    // vi.fn
  std::string vkey;         // vi.fvkey
  std::vector<std::string> urls;   // đã ghép sẵn base + fn + vkey
};

struct PlaybackResult {
  playback::FullRequest request;
  std::string request_text;     // dạng văn bản để lưu/replay
  std::string response_text;    // thân phản hồi đã giải gzip
  util::Json decoded;           // JSON đã giải ANC (nếu có)

  int em = -1;
  int exem = 0;
  std::string message;
  int login = -1;               // -1 = phản hồi không có trường này
  bool anc_decoded = false;
  bool session_refreshed = false;
  std::string error_hint;       // != "" khi em != 0: diễn giải sẵn cho người dùng

  std::vector<PlaybackStream> streams;

  // Ngữ cảnh cần cho bước tải tiếp theo (đi kèm account, không phải hằng số).
  std::string download_user_agent;
  download_proxy::Context download_proxy_context;
  std::string account_vuid;
};

class PlaybackService {
 public:
  explicit PlaybackService(AccountServiceOptions options = AccountServiceOptions())
      : accounts_(options) {}

  // Chỉ dựng request (không chạm mạng) — dùng cho --dry-run và cho test.
  Result<PlaybackResult> build(const PlaybackRequest& request) const;

  // Dựng + gửi + phân tích phản hồi.
  Result<PlaybackResult> fetch(const PlaybackRequest& request) const;

  // Chuỗi văn bản của một request (dòng đầu METHOD URL, headers, dòng trống,
  // body). Đây là định dạng mà lệnh `download` đọc lại.
  static std::string serialize(const playback::FullRequest& request);

 private:
  AccountService accounts_;
};

}  // namespace tencent::service
