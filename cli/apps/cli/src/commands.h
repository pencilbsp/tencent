// commands.h — Khai báo tham số và điểm vào của từng lệnh con.
//
// Mỗi lệnh gồm một struct tham số (do main.cpp điền từ dòng lệnh) và một hàm
// `run` trả về mã thoát. Nhờ tách như vậy:
//   • main.cpp chỉ còn việc khai báo cờ CLI11 và gọi hàm — không chứa nghiệp vụ;
//   • mỗi lệnh nằm gọn trong một file, dễ thêm/bớt lệnh mới;
//   • các struct tham số này là ranh giới rõ ràng giữa "cú pháp dòng lệnh" và
//     "yêu cầu nghiệp vụ" (service::*Request).
#pragma once

#include <string>
#include <vector>

#include "tencent/core/service/download_service.h"
#include "tencent/core/protocol/playback/capability.h"

namespace tencent::cli {

// ── auth ─────────────────────────────────────────────────────────────────────
struct AuthOptions {
  // Chế độ (đúng một trong ba, xác định bởi cờ nào xuất hiện trên dòng lệnh).
  enum class Mode { kDump, kRefresh, kInfo };
  Mode mode = Mode::kDump;

  std::string device_id;        // --adb [<device_id>]
  std::string account_id;       // --refresh/--info [<account_id>]
  std::string package;
  std::string from_prefs_file;  // --from-prefs (chạy ngoại tuyến)
  std::string output_path;      // --out
  bool use_device = true;       // false khi chỉ có --from-prefs
  bool skip_device_info = false;
  bool set_default = false;
  bool dry_run = false;         // refresh: chỉ dựng khung
  bool local_only = false;      // info: không gọi mạng
  bool allow_refresh = true;    // info: cho phép tự làm mới phiên
  long timeout_sec = 20;
};
int run_auth(const AuthOptions& options);

// ── list ─────────────────────────────────────────────────────────────────────
int run_list();

// ── getvinfo ─────────────────────────────────────────────────────────────────
struct GetvinfoOptions {
  std::string url;              // URL trang xem; tự tách ra cid/vid
  std::string vid;
  std::string cid;
  std::string account_id;
  std::string defn = "uhd";
  std::string mode = "normal";
  playback::CapabilityOptions capabilities;

  bool dry_run = false;
  bool print_raw_json = false;
  bool insecure = false;
  std::string proxy_url;
  long timeout_sec = 20;

  // Ghi lại phục vụ debug/replay.
  std::string request_out;
  std::string response_out;
  std::string decoded_out;

  // Tải luôn sau khi lấy được thông tin phát.
  std::string output_path;
  // Tham số tải dùng chung với lệnh `download` (chỉ đọc khi có -o/--output).
  service::DownloadRequest download;
};
int run_getvinfo(const GetvinfoOptions& options);

// ── download ─────────────────────────────────────────────────────────────────
struct DownloadOptions {
  service::DownloadRequest download;
  std::string account_id;       // để lấy User-Agent của DownloadProxy
};
int run_download(const DownloadOptions& options);

// ── probe-url ────────────────────────────────────────────────────────────────
struct ProbeOptions {
  std::string url;
  std::string proxy_url;
  std::vector<std::string> headers;   // dạng "Name: value"
  long timeout_sec = 20;
  bool insecure = false;
};
int run_probe(const ProbeOptions& options);

// ── flush ────────────────────────────────────────────────────────────────────
struct FlushOptions {
  std::string work_dir;
  std::string vid;
  std::string defn;
  bool dry_run = false;
};
int run_flush(const FlushOptions& options);

// ── Tiện ích dùng chung cho các lệnh ────────────────────────────────────────

// Tách cid/vid từ URL trang xem dạng https://v.qq.com/x/cover/<cid>/<vid>.html
// Trả false (kèm thông báo đã in) nếu URL không đúng dạng hoặc mâu thuẫn với
// --vid/--cid mà người dùng truyền vào.
bool resolve_ids_from_url(std::string& vid, std::string& cid, const std::string& url);

}  // namespace tencent::cli
