// probe_command.cpp — Lệnh `probe-url`: gửi một GET để kiểm tra mạng/proxy.
//
// Công cụ chẩn đoán: đo thời gian và tốc độ tải một URL, có thể đi qua proxy
// ss:// giống hệt đường mà pipeline tải dùng.
#include "../commands.h"
#include "../console.h"

#include <cstdio>

#include "tencent/core/common/strings.h"
#include "tencent/core/service/diagnostics_service.h"

namespace tencent::cli {

int run_probe(const ProbeOptions& options) {
  service::ProbeRequest request;
  request.url = options.url;
  request.proxy_url = options.proxy_url;
  request.timeout_sec = options.timeout_sec;
  request.insecure = options.insecure;

  for (const auto& raw : options.headers) {
    const std::size_t separator = raw.find(':');
    if (separator == std::string::npos) {
      print_error("header sai định dạng, cần 'Name: value': " + raw);
      return 2;
    }
    request.headers.push_back({util::trim(raw.substr(0, separator)),
                               util::trim(raw.substr(separator + 1))});
  }

  const service::DiagnosticsService service;
  auto result = service.probe_url(request);
  if (!result) return report(result.error());

  const service::ProbeResult& probe = result.value();
  char line[256];
  std::snprintf(line, sizeof(line), "status=%ld size=%llu time=%.3fs speed=%s/s",
                probe.http_status, static_cast<unsigned long long>(probe.size_bytes),
                probe.seconds,
                format_bytes(static_cast<uint64_t>(probe.bytes_per_second)).c_str());
  print(line);
  // Mã 2xx = đường đi thông; còn lại để người dùng tự diễn giải.
  return probe.http_status >= 200 && probe.http_status < 300 ? 0 : 1;
}

}  // namespace tencent::cli
