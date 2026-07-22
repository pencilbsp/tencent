// diagnostics_service.cpp — Xem include/tencent/core/service/diagnostics_service.h.
#include "tencent/core/service/diagnostics_service.h"

#include <chrono>

#include "tencent/core/net/http_client.h"
#include "tencent/core/service/error.h"

namespace tencent::service {

Result<ProbeResult> DiagnosticsService::probe_url(const ProbeRequest& request) const {
  if (request.url.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "thiếu URL cần thử");
  }
  return guard<ProbeResult>(ErrorCode::kNetwork, "probe thất bại", [&] {
    const auto start = std::chrono::steady_clock::now();
    auto [status, body] = http::http_get(request.url, request.headers, request.timeout_sec,
                                         request.insecure, request.proxy_url);
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    ProbeResult result;
    result.http_status = status;
    result.size_bytes = body.size();
    result.seconds = seconds;
    result.bytes_per_second = seconds > 0.0 ? static_cast<double>(body.size()) / seconds : 0.0;
    result.body = std::move(body);
    return result;
  });
}

}  // namespace tencent::service
