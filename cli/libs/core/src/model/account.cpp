// account.cpp — Xem include/tencent/core/model/account.h.
//
// Chỉ chứa DỮ LIỆU + PHÂN TÍCH: parse cookie, dò cookie/JSON trong dump MMKV,
// dựng Cookie đăng nhập và ánh xạ sang SessionMaterial. Phần đọc/ghi đĩa nằm ở
// storage/account_store.cpp, phần đường dẫn ở storage/paths.cpp.
#include "tencent/core/model/account.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "tencent/core/common/json.h"
#include "tencent/core/common/strings.h"

namespace tencent::account {

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

// ── helpers ──────────────────────────────────────────────────────────────────
static std::string pick(const std::map<std::string, std::string>& c,
                        std::initializer_list<const char*> keys) {
  for (const char* k : keys) {
    auto it = c.find(k);
    if (it != c.end() && !it->second.empty() && it->second != "null") return it->second;
  }
  return "";
}

// HTML unescape tối thiểu (XML prefs escape &amp;/&lt;/&gt;/&quot;/&apos;).
static std::string html_unescape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (std::size_t i = 0; i < s.size();) {
    if (s[i] == '&') {
      if (s.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 5; continue; }
      if (s.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 4; continue; }
      if (s.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 4; continue; }
      if (s.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 6; continue; }
      if (s.compare(i, 6, "&apos;") == 0) { out.push_back('\''); i += 6; continue; }
      if (s.compare(i, 5, "&#39;") == 0) { out.push_back('\''); i += 5; continue; }
    }
    out.push_back(s[i++]);
  }
  return out;
}

// util::trim() dùng bản chung util::trim (common/strings.h).

// ── parse_cookie ─────────────────────────────────────────────────────────────
std::string derive_build(const std::string& app_ver) {
  const std::size_t dot = app_ver.find_last_of('.');
  return dot == std::string::npos ? "" : app_ver.substr(dot + 1);
}

std::map<std::string, std::string> parse_cookie(const std::string& cookie) {
  std::map<std::string, std::string> out;
  std::size_t i = 0;
  while (i < cookie.size()) {
    std::size_t semi = cookie.find(';', i);
    std::string part = cookie.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
    std::size_t eq = part.find('=');
    if (eq != std::string::npos) {
      std::string k = util::trim(part.substr(0, eq));
      std::string v = util::trim(part.substr(eq + 1));
      if (!k.empty()) out[k] = v;
    }
    if (semi == std::string::npos) break;
    i = semi + 1;
  }
  return out;
}

// ── fill_from_cookie (mapping = ../analytics/app/login_refresh.py:load_refresh_inputs) ──
void fill_from_cookie(Account& a, const std::map<std::string, std::string>& c) {
  a.vuid = pick(c, {"vuserid", "v_vuserid"});
  a.vusession = pick(c, {"vusession", "v_vusession"});
  a.vurefresh = pick(c, {"v_vurefresh", "vurefresh"});
  a.wx_app_id = pick(c, {"appid", "v_t_appid"});
  a.wx_open_id = pick(c, {"openid", "v_t_openid"});
  a.wx_access_token = pick(c, {"access_token", "v_t_access_token"});
  a.wx_refresh_token = pick(c, {"refresh_token", "v_t_refresh_token"});
  a.qimei36 = pick(c, {"vdevice_qimei36", "qimei36", "qimei"});
  a.guid = pick(c, {"guid"});
  a.usid = pick(c, {"usid"});
  // Device gợi ý từ cookie (nếu có) — getprop/dumpsys ghi đè sau.
  if (a.app_ver.empty()) a.app_ver = pick(c, {"app_ver"});
}

// ── find_login_cookie_in_prefs ───────────────────────────────────────────────
std::string find_login_cookie_in_prefs(const std::string& blob) {
  std::string best;
  std::size_t pos = 0;
  while ((pos = blob.find("<string", pos)) != std::string::npos) {
    std::size_t gt = blob.find('>', pos);
    if (gt == std::string::npos) break;
    std::size_t close = blob.find("</string>", gt);
    if (close == std::string::npos) break;
    std::string inner = html_unescape(blob.substr(gt + 1, close - gt - 1));
    // marker: phải có cả vusession= và vdevice_qimei36= để chắc là cookie login.
    if (inner.find("vusession=") != std::string::npos &&
        inner.find("vdevice_qimei36=") != std::string::npos) {
      if (inner.size() > best.size()) best = inner;  // candidate đầy đủ nhất.
    }
    pos = close + 9;
  }
  return best;
}

// ── find_login_cookie_in_binary (MMKV) ───────────────────────────────────────
std::string find_login_cookie_in_binary(const std::string& blob) {
  std::string best, run;
  auto consider = [&](const std::string& r) {
    if (r.size() >= 24 && r.find("vusession=") != std::string::npos &&
        r.find("vdevice_qimei36=") != std::string::npos && r.size() > best.size())
      best = r;
  };
  for (unsigned char c : blob) {
    if (c >= 32 && c <= 126) run.push_back(static_cast<char>(c));
    else { consider(run); run.clear(); }
  }
  consider(run);
  return best;
}

std::string find_login_cookie(const std::string& blob) {
  std::string c = find_login_cookie_in_prefs(blob);
  if (c.empty()) c = find_login_cookie_in_binary(blob);
  return c;
}

// Như find_login_cookie_in_binary nhưng ưu tiên run có vuserid=<vuid>.
std::string find_login_cookie_for_vuid(const std::string& blob, const std::string& vuid) {
  std::string want1 = "vuserid=" + vuid, want2 = "v_vuserid=" + vuid;
  std::string best, match, run;
  auto consider = [&](const std::string& r) {
    if (r.size() < 24 || r.find("vusession=") == std::string::npos ||
        r.find("vdevice_qimei36=") == std::string::npos)
      return;
    if (r.size() > best.size()) best = r;
    if (!vuid.empty() && (r.find(want1) != std::string::npos ||
                          r.find(want2) != std::string::npos) &&
        r.size() > match.size())
      match = r;
  };
  for (unsigned char c : blob) {
    if (c >= 32 && c <= 126) run.push_back(static_cast<char>(c));
    else { consider(run); run.clear(); }
  }
  consider(run);
  return match.empty() ? best : match;
}

// ── parse_account_json (NGUỒN CHUẨN) ─────────────────────────────────────────
bool parse_account_json(const std::string& blob, Account& a) {
  using njson = nlohmann::json;
  bool found = false, best_wx = false;
  int64_t best_ctime = -1;
  std::size_t i = 0, n = blob.size();
  while (i < n) {
    if (blob[i] != '{') { ++i; continue; }
    // Khớp ngoặc (tôn trọng string literal + escape), giới hạn 8KB/object.
    int depth = 0;
    bool instr = false, esc = false, ok = false;
    std::size_t j = i, lim = std::min(n, i + 8000);
    for (; j < lim; ++j) {
      char ch = blob[j];
      if (instr) {
        if (esc) esc = false;
        else if (ch == '\\') esc = true;
        else if (ch == '"') instr = false;
      } else if (ch == '"') instr = true;
      else if (ch == '{') ++depth;
      else if (ch == '}') { if (--depth == 0) { ok = true; ++j; break; } }
    }
    if (!ok) { ++i; continue; }
    std::string cand = blob.substr(i, j - i);
    i = j;
    njson o;
    try { o = njson::parse(cand); } catch (...) { continue; }
    if (!o.is_object() || (!o.contains("videoUserId") && !o.contains("videoSessionKey")))
      continue;
    bool is_wx = o.value("accountType", -1) == 1;
    int64_t ctime = o.value("videoCreateTime", static_cast<int64_t>(0));
    // Ưu tiên WX; trong cùng loại chọn videoCreateTime lớn nhất (mới nhất).
    bool better = (is_wx && !best_wx) || (is_wx == best_wx && ctime > best_ctime);
    if (!better) continue;

    found = true; best_wx = is_wx; best_ctime = ctime;
    int64_t vuid_n = o.value("videoUserId", static_cast<int64_t>(0));
    if (vuid_n) a.vuid = std::to_string(vuid_n);
    a.vusession = o.value("videoSessionKey", std::string());
    a.vurefresh = o.value("videoRefreshKey", std::string());
    a.wx_app_id = o.value("appid", std::string());
    a.wx_open_id = o.value("openId", std::string());
    a.wx_access_token = o.value("accessToken", std::string());
    a.wx_refresh_token = o.value("refreshToken", std::string());
    a.nickname = o.value("nickName", std::string());
    a.head_url = o.value("headImgUrl", std::string());
    int64_t vexp = o.value("videoExpireTime", static_cast<int64_t>(0));
    if (ctime && vexp) a.vusession_expire_ts = (ctime + vexp) / 1000;  // ms → giây
  }
  return found;
}

// ── extract_guid (key_guid trong MMKV) ───────────────────────────────────────
std::string extract_guid(const std::string& blob) {
  auto is_hex = [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  };
  std::size_t p = 0;
  while ((p = blob.find("key_guid", p)) != std::string::npos) {
    std::size_t start = p + 8, end = std::min(blob.size(), start + 64);
    for (std::size_t k = start; k + 32 <= end; ++k) {
      bool all_hex = true;
      for (std::size_t t = 0; t < 32; ++t)
        if (!is_hex(blob[k + t])) { all_hex = false; break; }
      // UUID v1: nibble version (ký tự thứ 13) = '1'.
      if (all_hex && blob[k + 12] == '1') return blob.substr(k, 32);
    }
    p += 8;
  }
  return "";
}

std::string build_login_cookie(const Account& a, int64_t now_ms) {
  std::string app_ver = a.app_ver.empty() ? "9.02.90.31109" : a.app_ver;
  std::string us_stamp;
  if (a.usid.size() >= 13 &&
      std::all_of(a.usid.begin(), a.usid.begin() + 13,
                  [](unsigned char c) { return std::isdigit(c); }))
    us_stamp = a.usid.substr(0, 13);
  if (us_stamp.empty())
    us_stamp = std::to_string(a.updated_at > 0 ? a.updated_at * 1000 : now_ms);
  std::string now = std::to_string(now_ms);
  std::string login_time = std::to_string(a.updated_at > 0 ? a.updated_at : now_ms / 1000);

  std::ostringstream out;
  auto add = [&](const char* key, const std::string& value) {
    out << key << '=' << value << ';';
  };
  add("app_ver", app_ver);
  add("main_login", "wx");
  add("video_platform", "3");
  add("video_appid", "1000005");
  add("vdevice_qimei36", a.qimei36);
  add("video_omgid", "");
  add("guid", a.guid);
  add("usid", a.usid);
  add("us_stamp", us_stamp);
  add("ussn", "0");
  add("ctime", now);
  add("zdtime", now);
  add("vcuid", "");
  add("vuserid", a.vuid);
  add("vusession", a.vusession);
  add("vqq_appid", "");
  add("vqq_openid", "");
  add("vqq_access_token", "");
  add("vqq_vuserid", "");
  add("vqq_vusession", "");
  add("vqq_refresh_token", "");
  add("appid", a.wx_app_id);
  add("openid", a.wx_open_id);
  add("access_token", a.wx_access_token);
  add("refresh_token", a.wx_refresh_token);
  add("role", "0");
  add("p_vuserid", "0");
  add("v_main_login", "wx");
  add("v_vuserid", a.vuid);
  add("v_vusession", a.vusession);
  add("v_vurefresh", a.vurefresh);
  add("v_t_appid", a.wx_app_id);
  add("v_t_openid", a.wx_open_id);
  add("v_t_access_token", a.wx_access_token);
  add("v_t_refresh_token", a.wx_refresh_token);
  add("v_role", "0");
  add("v_p_vuserid", "0");
  int64_t refresh_seconds =
      a.vusession_expire_ts > now_ms / 1000
          ? a.vusession_expire_ts - now_ms / 1000
          : 0;
  add("v_next_refresh_time", std::to_string(refresh_seconds));
  add("v_login_time_init", login_time);
  return out.str();
}

// ── to_session ───────────────────────────────────────────────────────────────
model::SessionMaterial Account::to_session() const {
  model::SessionMaterial s;
  auto set = [&](const char* k, const std::string& v) { if (!v.empty()) s.kv[k] = v; };
  // identity / credential / session.
  set("vuid", vuid);
  set("vusession", vusession);
  set("vurefresh", vurefresh);
  set("wx_app_id", wx_app_id);
  set("wx_open_id", wx_open_id);
  set("wx_access_token", wx_access_token);
  set("wx_refresh_token", wx_refresh_token);
  set("qimei36", qimei36);
  set("guid", guid);
  set("usid", usid);
  // Device/app context bắt buộc cho TRPC head.
  set("model", model);
  set("brand", brand);
  set("manufacturer", manufacturer);
  set("os_version", os_version);
  set("android_api_level", android_api_level);
  set("app_ver", app_ver);
  set("app_build", app_build);
  set("app_sub_build", app_sub_build);
  set("platform", platform);
  set("sdtfrom", sdtfrom);
  if (screen_width > 0) s.kv["screen_width"] = std::to_string(screen_width);
  if (screen_height > 0) s.kv["screen_height"] = std::to_string(screen_height);
  if (screen_dpi > 0) s.kv["screen_dpi"] = std::to_string(screen_dpi);
  return s;
}

std::string missing_required(const Account& a) {
  std::string m;
  auto add = [&](bool miss, const char* n) {
    if (miss) { if (!m.empty()) m += ", "; m += n; }
  };
  add(a.vuid.empty(), "vuid");
  add(a.vusession.empty(), "vusession");
  add(a.vurefresh.empty(), "vurefresh");
  add(a.wx_app_id.empty(), "wx_app_id");
  add(a.wx_open_id.empty(), "wx_open_id");
  add(a.wx_access_token.empty(), "wx_access_token");
  add(a.wx_refresh_token.empty(), "wx_refresh_token");
  add(a.qimei36.empty(), "qimei36");
  add(a.guid.empty(), "guid");
  add(a.usid.empty(), "usid");
  add(a.model.empty(), "device.model");
  add(a.brand.empty(), "device.brand");
  add(a.os_version.empty(), "device.os_version");
  add(a.android_api_level.empty(), "device.android_api_level");
  add(a.app_ver.empty(), "device.app_ver");
  add(a.app_build.empty(), "device.app_build");
  add(a.app_sub_build.empty(), "device.app_sub_build");
  add(a.platform.empty(), "device.platform");
  add(a.screen_width <= 0, "device.screen_width");
  add(a.screen_height <= 0, "device.screen_height");
  add(a.screen_dpi <= 0, "device.screen_dpi");
  return m;
}

}  // namespace tencent::account
