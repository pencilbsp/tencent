// account_store.cpp — Xem include/tencent/core/storage/account_store.h.
//
// Đọc/ghi bundle account trên đĩa: auth.json + fingerprint.json (schema v2),
// đọc được cả file monolith <vuid>.json (schema v1) rồi tự chuyển đổi ở lần ghi
// kế tiếp. Toàn bộ hiểu biết về ĐỊNH DẠNG FILE nằm ở đây — model/account.h chỉ
// mô tả dữ liệu, không biết nó được lưu ra sao.
#include "tencent/core/storage/account_store.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "tencent/core/common/json.h"
#include "tencent/core/common/strings.h"
#include "tencent/core/storage/paths.h"
#include "tencent/core/common/hex.h"

namespace tencent::storage {

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;
using account::Account;
using account::kSchemaVersion;

// ── I/O JSON ─────────────────────────────────────────────────────────────────
static std::string jstr(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return "";
  if (it->is_string()) return it->get<std::string>();
  if (it->is_number_integer()) return std::to_string(it->get<long long>());
  return "";
}
static int64_t jint(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return 0;
  if (it->is_number()) return it->get<int64_t>();
  if (it->is_string()) { try { return std::stoll(it->get<std::string>()); } catch (...) {} }
  return 0;
}
static std::vector<int> jint_array(const json& j, const char* key) {
  std::vector<int> out;
  auto it = j.find(key);
  if (it == j.end() || !it->is_array()) return out;
  for (const auto& v : *it) {
    if (v.is_number_integer()) out.push_back(v.get<int>());
    else if (v.is_string()) {
      try { out.push_back(std::stoi(v.get<std::string>())); } catch (...) {}
    }
  }
  return out;
}

static json read_json_file(const fs::path& path, const char* kind) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error(std::string("Unable to open ") + kind + ": " + path.string());
  std::ostringstream ss;
  ss << in.rdbuf();
  try {
    return json::parse(ss.str());
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to parse ") + kind + ": " + e.what());
  }
}

// Quyền chỉ-chủ-sở-hữu dùng bản chung ở storage/paths.h
// (trước đây file này có bản sao riêng).

static void write_json_file(const fs::path& path, const json& j) {
  fs::create_directories(path.parent_path());
  fs::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Unable to write account: " + path.string());
    set_owner_only_file(tmp.string());
    out << j.dump(2) << "\n";
    if (!out) throw std::runtime_error("Failed to finish writing account: " + path.string());
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  if (ec) {
    fs::remove(path, ec);
    ec.clear();
    fs::rename(tmp, path, ec);
  }
  if (ec) {
    fs::remove(tmp);
    throw std::runtime_error("Could not replace account: " + path.string() + ": " + ec.message());
  }
  set_owner_only_file(path.string());
}

static json section(const json& j, const char* name) {
  auto it = j.find(name);
  return it != j.end() && it->is_object() ? *it : json::object();
}

static void load_auth_json(const json& j, Account& a) {
  a.schema_version = static_cast<int>(jint(j, "schema_version"));
  if (a.schema_version == 0) a.schema_version = kSchemaVersion;
  a.vuid = jstr(j, "vuid");
  a.created_at = jint(j, "created_at");
  a.updated_at = jint(j, "updated_at");
  json source = section(j, "source");
  a.source_method = jstr(source, "method");
  a.source_device_id = jstr(source, "device_id");
  a.source_pkg = jstr(source, "pkg");
  if (a.source_pkg.empty()) a.source_pkg = account::kDefaultPkg;

  json id = section(j, "identity");
  json cr = section(j, "credential");
  json se = section(j, "session");
  a.wx_app_id = jstr(id, "wx_app_id");
  a.wx_open_id = jstr(id, "wx_open_id");
  a.nickname = jstr(id, "nickname");
  a.head_url = jstr(id, "head_url");
  if (a.vuid.empty()) a.vuid = jstr(id, "vuid");
  a.vusession = jstr(cr, "vusession");
  a.vurefresh = jstr(cr, "vurefresh");
  a.wx_access_token = jstr(cr, "wx_access_token");
  a.wx_refresh_token = jstr(cr, "wx_refresh_token");
  a.vusession_expire_ts = jint(cr, "vusession_expire_ts");
  a.usid = jstr(se, "usid");
}

static Account load_legacy(const fs::path& path) {
  json j = read_json_file(path, "account legacy");
  Account a;
  load_auth_json(j, a);
  json id = section(j, "identity");
  json dv = section(j, "device");
  a.qimei36 = jstr(id, "qimei36");
  a.guid = jstr(id, "guid");
  a.model = jstr(dv, "model");
  a.brand = jstr(dv, "brand");
  a.manufacturer = jstr(dv, "manufacturer");
  a.os_version = jstr(dv, "os_version");
  a.android_api_level = jstr(dv, "android_api_level");
  a.app_ver = jstr(dv, "app_ver");
  a.app_build = jstr(dv, "app_build");
  a.app_sub_build = jstr(dv, "app_sub_build");
  a.download_proxy_version = jstr(dv, "download_proxy_version");
  a.platform = jstr(dv, "platform");
  a.sdtfrom = jstr(dv, "sdtfrom");
  a.screen_width = jint(dv, "screen_width");
  a.screen_height = jint(dv, "screen_height");
  a.screen_dpi = jint(dv, "screen_dpi");
  if (j.contains("turing") && j["turing"].is_object()) {
    const json& tu = j["turing"];
    a.turing.package_uid = jint(tu, "package_uid");
    a.turing.android_user_id = static_cast<int>(jint(tu, "android_user_id"));
    a.turing.k26_uuid_dashed = jstr(tu, "k26_uuid_dashed");
    a.turing.k26_wire = jstr(tu, "k26_wire");
    a.turing.k26_source = jstr(tu, "k26_source");
    a.turing.k26_cache_blob_hex = jstr(tu, "k26_cache_blob_hex");
    a.turing.k34_edb_state = static_cast<int>(jint(tu, "k34_edb_state"));
    a.turing.k34_load_bitmap = static_cast<int>(jint(tu, "k34_load_bitmap"));
    a.turing.k34_save_bitmap = static_cast<int>(jint(tu, "k34_save_bitmap"));
    std::vector<int> nrf = jint_array(tu, "k34_nrf");
    std::vector<int> nsf = jint_array(tu, "k34_nsf");
    if (!nrf.empty()) a.turing.k34_nrf = nrf;
    if (!nsf.empty()) a.turing.k34_nsf = nsf;
    a.turing.k34_ud = static_cast<int>(jint(tu, "k34_ud"));
    a.turing.k34_native_tail = jstr(tu, "k34_native_tail");
    a.turing.k34_cl_a = static_cast<int>(jint(tu, "k34_cl_a"));
    a.turing.k34_cl_c = static_cast<int>(jint(tu, "k34_cl_c"));
    a.turing.k34_cl = jstr(tu, "k34_cl");
    a.turing.k34_cl_cache_hex = jstr(tu, "k34_cl_cache_hex");
    a.turing.k34_r = jstr(tu, "k34_r");
    a.turing.k34_us_503 = static_cast<int>(jint(tu, "k34_us_503"));
    a.turing.k34_us_502 = static_cast<int>(jint(tu, "k34_us_502"));
    a.turing.k34_stif = jstr(tu, "k34_stif");
    if (tu.contains("k26_nodes") && tu["k26_nodes"].is_array()) {
      for (const auto& n : tu["k26_nodes"]) {
        if (!n.is_object()) continue;
        turing::K26Node node;
        node.index = static_cast<int>(jint(n, "index"));
        node.path = jstr(n, "path");
        node.size = jint(n, "size");
        node.read_status = static_cast<int>(jint(n, "read_status"));
        node.decode_status = static_cast<int>(jint(n, "decode_status"));
        node.uuid_dashed = jstr(n, "uuid_dashed");
        node.blob_hex = jstr(n, "blob_hex");
        a.turing.k26_nodes.push_back(node);
      }
    }
  }
  return a;
}

// pref_enabled/derive_build đã thành API dùng chung:
//   turing::pref_enabled (model/turing_material.h)
//   account::derive_build (model/account.h)

static void hydrate_fingerprint(const json& j, Account& a) {
  json identity = section(j, "identity");
  json client = section(j, "client");
  json private_data = section(j, "private");
  json local = section(j, "local");
  a.qimei36 = jstr(identity, "qimei36");
  a.guid = jstr(identity, "guid");
  a.platform = jstr(client, "platform");
  a.sdtfrom = jstr(client, "sdtfrom");
  a.download_proxy_version = jstr(client, "download_proxy_version");

  json raw = j.contains("raw") && j["raw"].is_object() ? j["raw"] : json::object();
  a.raw_fingerprint_json = raw.dump();
  json props = section(raw, "props");
  json device = section(raw, "device");
  json app = section(raw, "app");
  a.model = jstr(props, "ro.product.model");
  a.brand = jstr(props, "ro.product.brand");
  a.manufacturer = jstr(props, "ro.product.manufacturer");
  a.os_version = jstr(props, "ro.build.version.release");
  a.android_api_level = jstr(props, "ro.build.version.sdk");
  a.app_ver = jstr(app, "qqlive_version_name");
  a.app_sub_build = jstr(app, "qqlive_version_code");
  if (a.download_proxy_version.empty())
    a.download_proxy_version = jstr(app, "download_proxy_version");
  a.app_build = account::derive_build(a.app_ver);
  a.screen_width = jint(device, "screen_width");
  a.screen_height = jint(device, "screen_height");
  a.screen_dpi = jint(device, "screen_dpi");

  turing::Material& m = a.turing;
  m.package_uid = jint(app, "qqlive_uid");
  m.android_user_id = static_cast<int>(jint(app, "android_user_id"));
  if (m.android_user_id == 0 && m.package_uid >= 100000)
    m.android_user_id = static_cast<int>(m.package_uid / 100000);
  a.turing_prefs_xml = jstr(private_data, "turing_prefs_xml");
  auto prefs = turing::parse_turing_prefs_xml(a.turing_prefs_xml);
  std::vector<std::string> paths = turing::k26_cache_paths(a.source_pkg);
  const char* load_gate[5] = {"2011", "", "2009", "2009", ""};
  const char* save_gate[5] = {"2010", "", "2008", "2008", ""};
  json raw_nodes = private_data.contains("k26_nodes") &&
                           private_data["k26_nodes"].is_array()
                       ? private_data["k26_nodes"]
                       : json::array();
  bool loaded = false;
  for (std::size_t i = 0; i < paths.size(); ++i) {
    turing::K26Node node;
    node.index = static_cast<int>(i);
    node.path = paths[i];
    const json* stored = nullptr;
    for (const auto& candidate : raw_nodes) {
      if (candidate.is_object() &&
          static_cast<std::size_t>(jint(candidate, "index")) == i) {
        stored = &candidate;
        break;
      }
    }
    if (stored) {
      std::string stored_path = jstr(*stored, "path");
      if (!stored_path.empty()) node.path = stored_path;
      node.size = jint(*stored, "size");
      node.blob_hex = jstr(*stored, "blob_hex");
    }
    bool gated = load_gate[i][0] != '\0' && !turing::pref_enabled(prefs, load_gate[i]);
    if (gated) {
      node.read_status = -1;
      node.decode_status = -1;
    } else if (node.blob_hex.empty()) {
      node.read_status = stored ? static_cast<int>(jint(*stored, "read_status")) : -11002;
      if (node.read_status == 0) node.read_status = -11002;
      node.decode_status = node.read_status;
    } else {
      try {
        std::string blob = util::hex_to_string(node.blob_hex);
        node.size = static_cast<int64_t>(blob.size());
        node.read_status = 0;
        turing::K26Decoded decoded = turing::decode_k26_cache(blob);
        node.decode_status = decoded.status;
        node.uuid_dashed = decoded.uuid_dashed;
      } catch (...) {
        node.read_status = 0;
        node.decode_status = -2002;
      }
    }
    if (!loaded && !gated) {
      m.k34_nrf[i] = node.decode_status;
      if (node.decode_status == 0 && !node.uuid_dashed.empty()) {
        loaded = true;
        m.k26_uuid_dashed = node.uuid_dashed;
        m.k26_wire = turing::k26_wire_from_uuid(node.uuid_dashed);
        m.k26_source = "root-cache";
        m.k26_cache_blob_hex = node.blob_hex;
        m.k34_load_bitmap |= (1 << static_cast<int>(i));
      }
    }
    m.k26_nodes.push_back(node);
  }

  m.k34_edb_state = loaded ? 1 : 2;
  a.k26_save_status_raw = jint_array(private_data, "k26_save_status");
  for (std::size_t i = 0; i < paths.size(); ++i) {
    bool gated = save_gate[i][0] != '\0' && !turing::pref_enabled(prefs, save_gate[i]);
    if (gated) {
      m.k34_nsf[i] = -1;
    } else {
      m.k34_nsf[i] = i < a.k26_save_status_raw.size()
                         ? a.k26_save_status_raw[i]
                         : -40002;
      if (m.k34_nsf[i] == 0) m.k34_save_bitmap |= (1 << static_cast<int>(i));
    }
  }
  m.k34_ud = m.android_user_id;

  m.k34_cl_cache_hex = jstr(private_data, "k34_cl_cache_hex");
  if (!m.k34_cl_cache_hex.empty()) {
    try {
      auto cl = turing::decode_k34_cl_cache(util::hex_to_string(m.k34_cl_cache_hex));
      if (cl) {
        m.k34_cl_a = cl->first;
        m.k34_cl_c = cl->second;
        m.k34_cl = std::to_string(m.k34_cl_a) + "_" + std::to_string(m.k34_cl_c);
      }
    } catch (...) {}
  }
  auto get_pref = [&](const char* key) {
    auto it = prefs.find(key);
    return it == prefs.end() ? std::string() : it->second;
  };
  m.k34_r = get_pref("204");
  try { m.k34_us_503 = std::stoi(get_pref("503")); } catch (...) {}
  try { m.k34_us_502 = std::stoi(get_pref("502")); } catch (...) {}
  m.k34_stif = get_pref("904");

  if (m.k26_wire.empty()) {
    m.k26_uuid_dashed = jstr(local, "k26_uuid_dashed");
    m.k26_wire = jstr(local, "k26_wire");
    m.k26_source = jstr(local, "k26_source");
    if (m.k26_wire.empty() && !m.k26_uuid_dashed.empty()) {
      try { m.k26_wire = turing::k26_wire_from_uuid(m.k26_uuid_dashed); } catch (...) {}
    }
  }
  m.k34_native_tail = turing::build_k34_native_tail(
      m.k34_edb_state, m.k34_load_bitmap, m.k34_save_bitmap,
      m.k34_nrf, m.k34_nsf, m.k34_ud);

  // A one-time compatibility envelope preserves old monoliths that have no
  // helper raw snapshot. New adb bundles never need or emit derived fields.
  json compat = section(j, "legacy_derived");
  if (!compat.empty() && raw.empty()) {
    a.model = jstr(compat, "model");
    a.brand = jstr(compat, "brand");
    a.manufacturer = jstr(compat, "manufacturer");
    a.os_version = jstr(compat, "os_version");
    a.android_api_level = jstr(compat, "android_api_level");
    a.app_ver = jstr(compat, "app_ver");
    a.app_build = jstr(compat, "app_build");
    a.app_sub_build = jstr(compat, "app_sub_build");
    a.screen_width = jint(compat, "screen_width");
    a.screen_height = jint(compat, "screen_height");
    a.screen_dpi = jint(compat, "screen_dpi");
    json tu = section(compat, "turing");
    if (!tu.empty()) {
      m.package_uid = jint(tu, "package_uid");
      m.android_user_id = static_cast<int>(jint(tu, "android_user_id"));
      m.k34_native_tail = jstr(tu, "k34_native_tail");
      m.k34_cl = jstr(tu, "k34_cl");
      m.k34_r = jstr(tu, "k34_r");
      m.k34_us_503 = static_cast<int>(jint(tu, "k34_us_503"));
      m.k34_us_502 = static_cast<int>(jint(tu, "k34_us_502"));
      m.k34_stif = jstr(tu, "k34_stif");
    }
  }
}

Account load_account(const std::string& path) {
  fs::path p(path);
  if (fs::is_directory(p)) {
    Account a;
    load_auth_json(read_json_file(p / "auth.json", "auth.json"), a);
    fs::path fingerprint = p / "fingerprint.json";
    if (fs::exists(fingerprint))
      hydrate_fingerprint(read_json_file(fingerprint, "fingerprint.json"), a);
    return a;
  }
  if (fs::is_regular_file(p)) return load_legacy(p);
  fs::path legacy = p;
  legacy += ".json";
  if (fs::is_regular_file(legacy)) return load_legacy(legacy);
  throw std::runtime_error("Account not found: " + path);
}

void save_account(const std::string& path, const Account& a) {
  fs::path dir(path);
  if (fs::exists(dir) && !fs::is_directory(dir))
    throw std::runtime_error("Account path must be a directory: " + path);
  fs::create_directories(dir);
  set_owner_only_directory(dir.string());
  fs::path legacy = dir;
  legacy += ".json";
  if (fs::is_regular_file(legacy)) set_owner_only_file(legacy.string());

  json auth;
  auth["schema"] = "tencent-auth/1";
  auth["schema_version"] = kSchemaVersion;
  auth["vuid"] = a.vuid;
  auth["created_at"] = a.created_at;
  auth["updated_at"] = a.updated_at;
  auth["source"] = {{"method", a.source_method},
                    {"device_id", a.source_device_id},
                    {"pkg", a.source_pkg}};
  auth["identity"] = {{"vuid", a.vuid},
                      {"wx_app_id", a.wx_app_id},
                      {"wx_open_id", a.wx_open_id},
                      {"nickname", a.nickname},
                      {"head_url", a.head_url}};
  auth["credential"] = {{"vusession", a.vusession},
                        {"vurefresh", a.vurefresh},
                        {"wx_access_token", a.wx_access_token},
                        {"wx_refresh_token", a.wx_refresh_token},
                        {"vusession_expire_ts", a.vusession_expire_ts}};
  auth["session"] = {{"usid", a.usid}};

  json raw = json::object();
  if (!a.raw_fingerprint_json.empty()) {
    try {
      json candidate = json::parse(a.raw_fingerprint_json);
      if (candidate.is_object()) raw = std::move(candidate);
    } catch (...) {}
  }
  json nodes = json::array();
  for (const auto& n : a.turing.k26_nodes) {
    nodes.push_back({{"index", n.index},
                     {"path", n.path},
                     {"size", n.size},
                     {"read_status", n.read_status},
                     {"blob_hex", n.blob_hex}});
  }
  json fingerprint;
  fingerprint["schema"] = "tencent-fingerprint/1";
  fingerprint["schema_version"] = kSchemaVersion;
  fingerprint["updated_at"] = a.updated_at;
  fingerprint["identity"] = {{"qimei36", a.qimei36}, {"guid", a.guid}};
  fingerprint["client"] = {{"platform", a.platform},
                           {"sdtfrom", a.sdtfrom},
                           {"download_proxy_version", a.download_proxy_version}};
  fingerprint["raw"] = raw;
  fingerprint["private"] = {
      {"turing_prefs_xml", a.turing_prefs_xml},
      {"k26_nodes", nodes},
      {"k26_save_status", a.k26_save_status_raw.empty()
                              ? a.turing.k34_nsf
                              : a.k26_save_status_raw},
      {"k34_cl_cache_hex", a.turing.k34_cl_cache_hex}};
  fingerprint["local"] = {
      {"k26_uuid_dashed", a.turing.k26_uuid_dashed},
      {"k26_wire", a.turing.k26_wire},
      {"k26_source", a.turing.k26_source}};

  if (raw.empty()) {
    fingerprint["legacy_derived"] = {
        {"model", a.model}, {"brand", a.brand},
        {"manufacturer", a.manufacturer}, {"os_version", a.os_version},
        {"android_api_level", a.android_api_level},
        {"app_ver", a.app_ver}, {"app_build", a.app_build},
        {"app_sub_build", a.app_sub_build},
        {"download_proxy_version", a.download_proxy_version},
        {"screen_width", a.screen_width}, {"screen_height", a.screen_height},
        {"screen_dpi", a.screen_dpi},
        {"turing", {
            {"package_uid", a.turing.package_uid},
            {"android_user_id", a.turing.android_user_id},
            {"k34_native_tail", a.turing.k34_native_tail},
            {"k34_cl", a.turing.k34_cl}, {"k34_r", a.turing.k34_r},
            {"k34_us_503", a.turing.k34_us_503},
            {"k34_us_502", a.turing.k34_us_502},
            {"k34_stif", a.turing.k34_stif}}}};
  }

  // Fingerprint first, auth last: readers never observe a fresh auth file with
  // a missing matching fingerprint after a normal write.
  write_json_file(dir / "fingerprint.json", fingerprint);
  write_json_file(dir / "auth.json", auth);
}

bool account_exists(const std::string& path) {
  fs::path p(path);
  if (fs::is_directory(p)) return fs::is_regular_file(p / "auth.json");
  if (fs::is_regular_file(p)) return true;
  p += ".json";
  return fs::is_regular_file(p);
}

}  // namespace tencent::storage
