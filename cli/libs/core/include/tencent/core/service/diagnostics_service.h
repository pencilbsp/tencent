// diagnostics_service.h — Công cụ chẩn đoán mạng.
//
// Tách riêng khỏi ba dịch vụ nghiệp vụ vì nó không thuộc luồng nào cả: mục đích
// là trả lời câu hỏi "đường mạng/proxy này có dùng được không" trước khi đổ lỗi
// cho phần tải hay phần xác thực.
//
// Có mặt ở tầng service (thay vì để CLI gọi thẳng net/http_client.h) để giữ
// đúng luật: giao diện chỉ nói chuyện với `tencent::service::*`.
#pragma once

#include <string>

#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/common/result.h"

namespace tencent::service {

struct ProbeRequest {
  std::string url;
  OrderedKV headers;
  std::string proxy_url;
  long timeout_sec = 20;
  bool insecure = false;
};

struct ProbeResult {
  long http_status = 0;
  uint64_t size_bytes = 0;
  double seconds = 0.0;        // thời gian tính từ lúc phát request
  double bytes_per_second = 0.0;
  std::string body;
};

class DiagnosticsService {
 public:
  // Gửi một GET và đo thời gian/tốc độ. Chỉ lỗi transport mới trả Error;
  // mã HTTP 4xx/5xx vẫn là kết quả hợp lệ để người dùng tự đọc.
  Result<ProbeResult> probe_url(const ProbeRequest& request) const;
};

}  // namespace tencent::service
