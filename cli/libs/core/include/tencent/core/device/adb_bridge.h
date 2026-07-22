// adb_bridge.h — Gọi `adb` để đọc dữ liệu từ thiết bị Android.
//
// Dùng cho `tencent-cli auth --adb`: liệt kê thiết bị, đọc thuộc tính hệ thống,
// lấy dữ liệu riêng của app (qua Tencent Helper, xem device/helper_probe.h).
// Đây là phần DUMP theo AGENTS.md §3 — không có gì byte-exact ở đây.
//
// KHÁC BẢN CŨ: mọi lệnh chạy qua tencent::platform::run_process, tức là exec
// thẳng `adb` với danh sách đối số RỜI, KHÔNG qua /bin/sh. Vì thế:
//   • serial thiết bị hay đường dẫn có ký tự lạ không thể chèn lệnh vào máy chủ;
//   • không cần `2>/dev/null` — stderr được thu riêng rồi bỏ qua;
//   • hàm quote (device_quote) chỉ còn dùng cho shell CỦA THIẾT BỊ.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tencent::adb {

// Serial của các thiết bị đang ONLINE (state == "device").
// Rỗng nếu chưa cài adb hoặc không có thiết bị nào.
std::vector<std::string> list_devices();

// `adb -s <device_id> <args...>`. Trả stdout (an toàn nhị phân); stderr bị bỏ.
// Ném std::runtime_error nếu không tìm thấy chương trình `adb`.
std::string run(const std::string& device_id, const std::vector<std::string>& args);

// `adb -s <id> shell <command>` — `command` là MỘT chuỗi do shell của THIẾT BỊ
// diễn giải (dùng device_quote cho từng đối số bên trong).
std::string shell(const std::string& device_id, const std::string& command);

// Như trên nhưng chạy dưới quyền root: `su -c '<command>'`. Cần máy đã root.
std::string su_shell(const std::string& device_id, const std::string& command);

// `adb -s <id> exec-out <command>` — BẮT BUỘC khi lấy dữ liệu nhị phân:
// `shell` sẽ dịch LF thành CRLF và làm hỏng dump MMKV.
std::string exec_out(const std::string& device_id, const std::string& command);

// Đọc & nối nội dung nhiều file (glob do shell thiết bị tự khai triển), chạy
// dưới root, qua exec-out để an toàn nhị phân.
std::string su_cat(const std::string& device_id, const std::string& glob_path);

// Bọc một đối số cho shell CỦA THIẾT BỊ ('...'\''...').
std::string device_quote(const std::string& value);

// `getprop <key>` (đã cắt khoảng trắng).
std::string getprop(const std::string& device_id, const std::string& key);

// versionName / versionCode lấy từ `dumpsys package <pkg>`.
struct AppVersion {
  std::string version_name;
  std::string version_code;
};
AppVersion app_version(const std::string& device_id, const std::string& package);

// UID của package (`cmd package list packages -U`, fallback dumpsys). 0 nếu lỗi.
int64_t package_uid(const std::string& device_id, const std::string& package);

// `wm size` → độ phân giải vật lý; cả hai bằng 0 nếu không đọc được.
void screen_size(const std::string& device_id, int64_t& width, int64_t& height);

// `wm density` → dpi; 0 nếu không đọc được.
int64_t screen_density(const std::string& device_id);

// adb có trên PATH và thiết bị đang online.
bool device_online(const std::string& device_id);

}  // namespace tencent::adb
