// account_file.h — Account bundle:
//   ~/.tencent/accounts/<vuid>/auth.json
//   ~/.tencent/accounts/<vuid>/fingerprint.json
//
// `auth.json`: vuid/WX identity, credential xoay vòng, usid và profile.
// `fingerprint.json`: qimei/guid, exact `turing-raw/1`, private raw K26/K34
// material và K26 local fallback. Device/Turing derived được dựng lại khi load.
//
// Ranh giới DUMP (AGENTS.md §3): toàn bộ trạng thái tài khoản/thiết bị đọc từ
// thiết bị qua adb, KHÔNG tái tạo bằng thuật toán. Engine C++ nạp file này →
// to_session() → trpc_head builder; thiếu device/app field thì từ chối request.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "tencent/core/protocol/trpc/head.h"
#include "tencent/core/model/turing_material.h"

namespace tencent::account {

inline constexpr int kSchemaVersion = 2;
inline constexpr const char* kDefaultPkg = "com.tencent.qqlive";

struct Account {
  int schema_version = kSchemaVersion;
  std::string vuid;
  int64_t created_at = 0;
  int64_t updated_at = 0;

  // source — xuất xứ bản dump (không bí mật).
  std::string source_method;      // "adb"
  std::string source_device_id;
  std::string source_pkg = kDefaultPkg;

  // identity (A) — tĩnh.
  std::string qimei36, guid, wx_app_id, wx_open_id;
  // App login accountType: QQ=0, WX=1, PHONE=2. -1 = legacy/unknown dump.
  int login_type = -1;

  // credential (B) — xoay vòng.
  std::string vusession, vurefresh, wx_access_token, wx_refresh_token;
  int64_t vusession_expire_ts = 0;

  // session (C).
  std::string usid;

  // profile (tiện hiển thị; từ account JSON).
  std::string nickname, head_url;

  // device — đi theo account (de-hardcode).
  std::string model, brand, manufacturer, os_version, android_api_level;
  std::string app_ver, app_build, app_sub_build, platform, sdtfrom;
  // Native DownloadProxy version. Official source in app:
  // TPDownloadProxyNative.getNativeVersion() -> native getVersion().
  std::string download_proxy_version;
  int64_t screen_width = 0, screen_height = 0, screen_dpi = 0;

  // Native Turing material — dump từ thiết bị để Mint có thể dựng request tươi
  // cùng trạng thái K26/K34/cache như app.
  turing::Material turing;
  std::string raw_fingerprint_json;  // Exact helper `turing-raw/1` object.
  std::string turing_prefs_xml;      // Raw private Turing prefs (root-only).
  std::vector<int> k26_save_status_raw;  // Helper save attempts before prefs gates.

  // Map sang model::SessionMaterial cho head builder: nạp cả identity/credential/session
  // LẪN đầy đủ device/app context bắt buộc.
  model::SessionMaterial to_session() const;
};

// app_build = đoạn sau dấu '.' cuối của app_ver (9.02.90.31109 → 31109).
std::string derive_build(const std::string& app_ver);

// ── Cookie login (chuỗi "k=v; k=v") ─────────────────────────────────────────
// Parse thành map (../analytics/app/app_data_cookies.py: cookie là 1 chuỗi trong prefs).
std::map<std::string, std::string> parse_cookie(const std::string& cookie);
// Điền identity/credential/session từ cookie fields. Mapping đối chiếu
// ../analytics/app/login_refresh.py:load_refresh_inputs (vuserid/v_vurefresh/v_t_*…).
void fill_from_cookie(Account& a, const std::map<std::string, std::string>& c);

// ── Quét shared_prefs XML đã pull (nối nhiều file) tìm chuỗi cookie ──────────
// Trả candidate dài nhất chứa cả marker vusession= + vdevice_qimei36= ("" nếu
// không thấy). ../analytics/app/app_data_cookies.py:find_login_cookie_values.
std::string find_login_cookie_in_prefs(const std::string& prefs_xml_blob);

// Quét MMKV/binary đã pull: lấy run printable (ASCII) dài nhất chứa cả 2 marker.
// TencentVideo lưu cookie login trong MMKV (files/mmkv/qad_core_sp_storage…),
// KHÔNG ở shared_prefs. ../analytics/app/app_data_cookies.py:_iter_printable_strings.
std::string find_login_cookie_in_binary(const std::string& blob);

// Thử cả 2 (XML trước, rồi binary) — tiện cho cả file pull lẫn dump offline.
std::string find_login_cookie(const std::string& blob);

// Chọn run cookie có vuserid=<vuid> (đúng account); fallback run dài nhất. Khử
// nhập nhằng khi MMKV có nhiều bản cookie (current/history/đa-account).
std::string find_login_cookie_for_vuid(const std::string& blob, const std::string& vuid);

// ── NGUỒN CHUẨN: account-state JSON trong MMKV (com.tencent.qqlive_preferences) ──
// Quét object JSON login (có videoUserId/videoSessionKey), chọn WX (accountType==1)
// MỚI NHẤT (max videoCreateTime), điền identity+credential+profile vào `a`. Trả
// true nếu tìm thấy. Đây là nguồn ĐÚNG cho vusession/vurefresh/WX tokens (đầy đủ),
// đối chiếu ../analytics/app/login_refresh.py:_freshest_wx_account + app_data_cookies.py.
bool parse_account_json(const std::string& blob, Account& a);

// guid = giá trị 32-hex (UUID v1) ngay sau key `key_guid` trong MMKV. guid KHÔNG
// sinh offline được (UUID v1 node/clock_seq NGẪU NHIÊN — xem ../analytics/app/guid_gen.py);
// PHẢI dump. Trả "" nếu không thấy.
std::string extract_guid(const std::string& blob);

// Dựng Cookie getvinfo theo LoginCookieUtils từ account/device hiện tại.
// ctime/zdtime được làm mới cho mỗi request; không cần cookie trong template.
std::string build_login_cookie(const Account& a, int64_t now_ms);

// ── I/O bundle ──────────────────────────────────────────────────────────────
// `path` is an account directory. `load` also accepts/migrates the legacy
// `<vuid>.json` monolith.
Account load(const std::string& path);
void save(const std::string& path, const Account& a);
bool exists(const std::string& path);

// Trả các trường BẮT BUỘC còn thiếu ("" = đủ). Nhóm WeChat chỉ bắt buộc khi
// account có dữ liệu WeChat; account đăng nhập kiểu khác không cần wx_*.
std::string missing_required(const Account& a);

// ── Đường dẫn & quản lý nhiều account ────────────────────────────────────────
std::string tencent_home();                              // $HOME/.tencent
std::string accounts_dir();                              // …/accounts
std::string config_path();                               // …/config.json
std::string account_path(const std::string& vuid);       // …/accounts/<vuid>/
std::string legacy_account_path(const std::string& vuid);// …/accounts/<vuid>.json
std::string backup_path(const std::string& vuid, int64_t ts);  // …/backups/<vuid>/<ts>/

void ensure_dirs();                                      // mkdir -p các thư mục
std::string get_default_account();                       // đọc config.json
void set_default_account(const std::string& vuid);       // ghi config.json

}  // namespace tencent::account
