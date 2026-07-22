// helper_probe.h — Nói chuyện với "Tencent Helper" (APK do người dùng tự cài).
//
// CLI KHÔNG tự cài APK và KHÔNG gọi `adb shell su`. Nó gửi một broadcast tường
// minh tới helper rồi đọc file kết quả trong vùng riêng của helper qua `run-as`.
// Nếu helper được cấp root, kết quả có thêm dữ liệu riêng của QQLive (cookie,
// account JSON, cache K26/K34); nếu không, các trường đó rỗng và phải tái dùng
// hoặc tổng hợp (xem apply_material_fallback).
//
// Vị trí trong kiến trúc: tầng `device/` = mọi thứ chạm tới thiết bị Android.
#pragma once

#include <string>
#include <vector>

#include "tencent/core/common/json.h"
#include "tencent/core/model/turing_material.h"

namespace tencent::turing {

struct ProbeOptions {
  std::string package = "com.turing.payloadprobe";
  std::string receiver = ".ProbeReceiver";
  std::string action = "com.turing.payloadprobe.COLLECT";
  std::string target_package = "com.tencent.qqlive";
  std::string response_root = "files/responses";
  int timeout_ms = 60000;  // Includes a possible root permission prompt.
  int poll_ms = 250;
};

struct HelperSnapshot {
  bool root_granted = false;
  std::string request_id;
  std::string raw_json;
  std::string account_blob;
  std::string prefs_blob;
  std::string turing_prefs_blob;
  std::vector<std::string> k26_blobs;
  std::string k34_cl_blob;
  std::vector<int> k26_save_status;
};

bool helper_installed(const std::string& device_id, const ProbeOptions& opt);

// Sends one explicit broadcast request and reads the response from helper
// private storage via run-as. The helper must already be installed by the user.
HelperSnapshot collect_helper_snapshot(const std::string& device_id,
                                       const ProbeOptions& opt);

// ── Ảnh chụp từ helper → vật liệu Turing lưu kèm account ────────────────────
//
// Tái dựng đúng trạng thái mà native thấy: duyệt 5 vị trí cache K26 theo thứ
// tự, tôn trọng các cờ prefs chặn đọc/ghi (2011/2009 cho đọc, 2010/2008 cho
// ghi), suy ra bitmap load/save và chuỗi tail K34.
// `raw` là JSON `turing-raw/1` do helper trả về.
Material material_from_snapshot(const HelperSnapshot& snapshot, const std::string& package,
                                const util::Json& raw);

// Bù phần thiếu khi không có root: ưu tiên K26 đã lưu của lần dump trước, nếu
// chưa từng có thì sinh mới (chỉ đúng cho thiết bị lần đầu). `previous` có thể
// là nullptr.
void apply_material_fallback(Material& material, const Material* previous);

}  // namespace tencent::turing
