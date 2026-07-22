// paths.h — Bố cục thư mục dữ liệu của công cụ (~/.tencent).
//
//   ~/.tencent/
//     config.json                     — account mặc định
//     accounts/<vuid>/auth.json       — identity + credential (nhóm A/B/C)
//     accounts/<vuid>/fingerprint.json— vân tay thiết bị (qimei/guid/Turing)
//     accounts/<vuid>.json            — ĐỊNH DẠNG CŨ (schema v1), vẫn đọc được
//     backups/<vuid>/<ts>/            — bản sao trước mỗi lần ghi đè credential
//     videos/<vid>/<defn>/            — cache tải video
//
// Gom về một chỗ để đổi vị trí lưu (hoặc cho GUI dùng thư mục khác) chỉ phải
// sửa một file. Thư mục gốc lấy từ tencent::platform (không đọc $HOME trực tiếp).
#pragma once

#include <cstdint>
#include <string>

namespace tencent::storage {

// Gốc dữ liệu. Mặc định = platform::default_app_data_directory() (~/.tencent).
std::string app_home();

// Ghi đè gốc dữ liệu (test, hoặc GUI muốn hồ sơ riêng). Chuỗi rỗng = trả về mặc định.
void set_app_home(const std::string& path);

std::string accounts_dir();                                    // …/accounts
std::string config_path();                                     // …/config.json
std::string account_path(const std::string& vuid);             // …/accounts/<vuid>
std::string legacy_account_path(const std::string& vuid);      // …/accounts/<vuid>.json
std::string backup_path(const std::string& vuid, int64_t timestamp);
std::string videos_dir();                                      // …/videos

// Tạo sẵn các thư mục và siết quyền về chủ sở hữu (0700/0600) — thư mục này
// chứa credential nên không được để người dùng khác trên máy đọc.
void ensure_dirs();

// Quyền chỉ-chủ-sở-hữu. Dùng cho mọi file/thư mục có dữ liệu nhạy cảm.
void set_owner_only_file(const std::string& path);
void set_owner_only_directory(const std::string& path);

// ── Account mặc định (config.json) ───────────────────────────────────────────
std::string default_account();                    // "" nếu chưa đặt
void set_default_account(const std::string& vuid);

}  // namespace tencent::storage
