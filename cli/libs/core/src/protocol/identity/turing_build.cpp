// turing_build.cpp — See turing_build.h. Ports analytics/app/turing_k_builders.py +
// analytics/app/turing_gather.py builders to run over the raw device JSON.
#include "tencent/core/protocol/identity/turing_build.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <random>
#include <set>

namespace tencent::turing {

namespace {
using nlohmann::json;

const json kEmptyObj = json::object();
const json kEmptyArr = json::array();

std::string js(const json& j, const char* key, const std::string& dflt = "") {
  if (!j.is_object() || !j.contains(key)) return dflt;
  const json& v = j.at(key);
  return v.is_string() ? v.get<std::string>() : dflt;
}
int64_t ji(const json& j, const char* key, int64_t dflt = 0) {
  std::string s = js(j, key);
  if (s.empty()) return dflt;
  try { return std::stoll(s); } catch (...) { return dflt; }
}
const json& jget(const json& j, const char* key) {
  if (j.is_object() && j.contains(key)) return j.at(key);
  return j.is_array() ? kEmptyArr : kEmptyObj;
}

std::string parent_dir(const std::string& path) {
  auto p = path.find_last_of('/');
  if (p == std::string::npos) return "";
  if (p == 0) return "/";
  return path.substr(0, p);
}

std::string fmt2(double v) {  // "%.2f"
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f", v);
  return buf;
}

// Java Float.toString(float): shortest round-trip mantissa (std::to_chars for
// float gives exactly that), with Java's notation rule (scientific iff |v|<1e-3
// or |v|>=1e7) and uppercase 'E'.
std::string java_float_tostring(float f) {
  if (f != f) return "NaN";
  if (f == std::numeric_limits<float>::infinity()) return "Infinity";
  if (f == -std::numeric_limits<float>::infinity()) return "-Infinity";
  if (f == 0.0f) return std::signbit(f) ? "-0.0" : "0.0";

  bool neg = f < 0;
  char buf[64];
  auto res = std::to_chars(buf, buf + sizeof(buf), neg ? -f : f,
                           std::chars_format::scientific);
  std::string sci(buf, res.ptr);   // e.g. "6.1086525e-04" or "6e-02"
  auto ep = sci.find('e');
  std::string mant = sci.substr(0, ep);
  int E = std::stoi(sci.substr(ep + 1));
  std::string sig;                  // significant digits, no '.'
  for (char c : mant) if (c != '.') sig += c;
  while (sig.size() > 1 && sig.back() == '0') sig.pop_back();

  std::string out;
  if (E < -3 || E >= 7) {           // scientific
    out = sig.substr(0, 1) + "." + (sig.size() > 1 ? sig.substr(1) : "0") +
          "E" + std::to_string(E);
  } else {                          // plain decimal
    int point = E + 1;              // digits left of the decimal point
    if (point <= 0) {
      out = "0." + std::string(-point, '0') + sig;
    } else if (point >= (int)sig.size()) {
      out = sig + std::string(point - sig.size(), '0') + ".0";
    } else {
      out = sig.substr(0, point) + "." + sig.substr(point);
    }
  }
  return neg ? "-" + out : out;
}

std::string build_k11(const json& raw) {
  std::string out;
  for (const auto& s : jget(raw, "sensors")) {
    int type = static_cast<int>(ji(s, "type"));
    if (type != 1 && type != 2 && type != 4 && type != 5 && type != 6) continue;
    uint32_t bits = static_cast<uint32_t>(ji(s, "resolution_bits"));
    float r;
    std::memcpy(&r, &bits, 4);
    out += "type=" + std::to_string(type) + ":name=" + js(s, "name") +
           ":vendor=" + js(s, "vendor") + ":resolution=" + java_float_tostring(r) + ",";
  }
  return out;
}

// ---- K builders ----

std::string build_k9(const json& raw) {
  std::string out;
  const json& df = jget(raw, "df");
  int n = 0;
  for (const auto& e : df) {
    if (n++ >= 8) break;
    out += js(e, "name") + ":" + js(e, "size") + ",";
  }
  return out;
}

std::string build_k10(const json& raw) {
  std::string out;
  bool first = true;
  for (const auto& e : jget(raw, "statfs")) {
    int64_t bs = ji(e, "block_size"), blocks = ji(e, "blocks");
    if (!first) out += ",";
    first = false;
    out += fmt2(static_cast<double>(bs) * static_cast<double>(blocks) / 1048576.0);
  }
  return out;
}

std::string build_k12(const json& raw) {
  std::string cpuinfo = js(jget(raw, "files"), "proc_cpuinfo");
  size_t pos = 0;
  while (pos < cpuinfo.size()) {
    size_t eol = cpuinfo.find('\n', pos);
    std::string line = cpuinfo.substr(pos, eol == std::string::npos ? eol : eol - pos);
    if (line.find("model name") != std::string::npos ||
        line.find("Hardware") != std::string::npos) {
      auto c = line.find(':');
      if (c != std::string::npos) {
        std::string v = line.substr(c + 1);
        size_t a = v.find_first_not_of(" \t");
        size_t b = v.find_last_not_of(" \t\r");
        return a == std::string::npos ? "" : v.substr(a, b - a + 1);
      }
    }
    if (eol == std::string::npos) break;
    pos = eol + 1;
  }
  return "";
}

std::string build_k16(const json& raw) {
  std::vector<uint64_t> hashes;
  for (const auto& e : jget(raw, "fonts")) {
    std::string name = js(e, "name");
    std::string size = js(e, "size");
    if (name.empty()) continue;
    hashes.push_back(murmur64a(name + ":" + size));
  }
  if (hashes.empty()) return "";
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(simhash64(hashes)));
  return buf;
}

std::string build_k17(const json& raw) {
  // seed order is the array order emitted by the probe (K17_SEED).
  struct Row { std::string name, suffix; int flag; int64_t uid, first, size; };
  std::vector<Row> rows;
  std::map<int64_t, int> install_counts;
  for (const auto& p : jget(raw, "packages")) {
    std::string name = js(p, "name");
    int64_t uid = ji(p, "uid"), size = ji(p, "apk_size");
    if (name.rfind("com.android", 0) == 0 || uid < 10000 || size <= 0) continue;
    std::string src = js(p, "source_dir");
    // normalize_data_app_suffix
    std::string suffix;
    if (src.rfind("/data/app/", 0) == 0) {
      std::string v;
      for (char c : src) if (c != ':' && c != ',') v += c;
      // remove all occurrences of the package name
      for (size_t pos; (pos = v.find(name)) != std::string::npos;) v.erase(pos, name.size());
      if (v.size() >= 9 && v.compare(v.size() - 9, 9, "/base.apk") == 0)
        v = v.substr(0, v.size() - 10);
      suffix = v.size() >= 6 ? v.substr(6) : "";
    }
    int flag = ji(p, "system") == 1 ? 0 : 1;
    int64_t first = ji(p, "first_install_ms");
    rows.push_back({name, suffix, flag, uid, first, size});
    install_counts[first]++;
  }
  std::string out;
  for (const auto& r : rows) {
    std::string second = install_counts[r.first] == 1 ? "1" : "0";
    out += std::to_string(r.flag) + ":" + second + ":" + std::to_string(r.uid) + ":" +
           std::to_string(r.first) + ":" + r.name + "::" + std::to_string(r.size) +
           ":" + r.suffix + ",";
  }
  return out;
}

// K18: replay the native /system DFS over the raw scan nodes.
struct K18Entry { std::string path; int64_t mtime, size; int flag; };

std::string build_k18(const json& raw) {
  const json& tree = jget(raw, "system_tree");
  struct Node { int dtype; int64_t mtime, size; bool readable; };
  std::map<std::string, Node> info;
  std::map<std::string, std::vector<std::string>> children;  // parent -> children (scan order)
  std::vector<std::string> order;
  for (const auto& e : tree) {
    std::string path = js(e, "path");
    if (path.empty()) continue;
    info[path] = {static_cast<int>(ji(e, "dtype")), ji(e, "mtime"), ji(e, "size"),
                  ji(e, "readable") != 0};
    children[parent_dir(path)].push_back(path);
    order.push_back(path);
  }
  // /system root: synthesize from its direct children if not scanned.
  if (!info.count("/system")) info["/system"] = {4, 0, 0, true};

  const int kMaxDepth = 4, kMaxEntries = 500;
  std::vector<K18Entry> out;
  std::map<std::string, std::set<int64_t>> seen_mtimes;

  std::function<void(const std::string&, int)> walk;
  auto add_dir = [&](const std::string& path, int count) {
    if ((int)out.size() >= kMaxEntries) return;
    auto it = info.find(path);
    if (it == info.end() || it->second.dtype != 4) return;
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    out.push_back({p + "/", it->second.mtime, 0, count});
  };
  auto add_file = [&](const std::string& path, int depth) {
    if ((int)out.size() >= kMaxEntries) return;
    auto it = info.find(path);
    if (it == info.end() || it->second.dtype == 10) return;  // skip symlink
    int64_t mtime = it->second.mtime;
    if (depth) {
      auto& bucket = seen_mtimes[parent_dir(path)];
      if (bucket.count(mtime)) return;
      bucket.insert(mtime);
    }
    out.push_back({path, mtime, it->second.size, 0});
  };
  walk = [&](const std::string& path, int depth) {
    if ((int)out.size() >= kMaxEntries) return;
    auto cit = children.find(path);
    if (cit == children.end() || cit->second.empty()) { add_dir(path, 0); return; }
    int child_count = 0;
    for (const auto& child : cit->second) {
      if ((int)out.size() >= kMaxEntries) break;
      child_count++;
      auto it = info.find(child);
      if (it == info.end()) continue;
      if (it->second.dtype == 10) continue;       // symlink
      if (it->second.dtype == 4) {
        int nd = depth + 1;
        if (!it->second.readable || nd >= kMaxDepth) add_dir(child, 0);
        else walk(child, nd);
      } else {
        add_file(child, depth);
      }
    }
    add_dir(path, child_count);
  };
  walk("/system", 1);

  std::sort(out.begin(), out.end(),
            [](const K18Entry& a, const K18Entry& b) { return a.path < b.path; });
  std::string s;
  int n = std::min((int)out.size(), kMaxEntries);
  for (int i = 0; i < n; i++) {
    if (i) s += ",";
    s += out[i].path + ":" + std::to_string(out[i].mtime) + ":" +
         std::to_string(out[i].size) + ":" + std::to_string(out[i].flag) + ":";
  }
  return s;
}

std::string build_k39(const json& raw) {
  static const std::vector<std::string> kOrder = {
      "product", "vendor", "system", "system_ext", "odm", "oem",
      "system_dlkm", "vendor_dlkm"};
  std::vector<std::string> pkgs;
  for (const auto& v : jget(raw, "rro_overlays"))
    if (v.is_string()) pkgs.push_back(v.get<std::string>());
  std::sort(pkgs.begin(), pkgs.end(), [](const std::string& a, const std::string& b) {
    auto rank = [](const std::string& p) {
      std::string part = p.substr(std::string("android.auto_generated_rro_").size());
      while (!part.empty() && part.back() == '_') part.pop_back();
      for (size_t i = 0; i < kOrder.size(); i++) if (kOrder[i] == part) return (int)i;
      return 99;
    };
    return rank(a) < rank(b);
  });
  if (pkgs.empty()) return "";
  std::string out;
  for (size_t i = 0; i < pkgs.size(); i++) out += (i ? ":" : "") + pkgs[i];
  return out + ",";
}

std::string build_k105(const json& raw) {
  const json& d = jget(raw, "device");
  int64_t level = ji(d, "battery_level", -1), scale = ji(d, "battery_scale", -1);
  if (scale <= 0 || level < 0) return "-1";
  int64_t pct = (level * 100) / scale;
  return std::to_string(std::max<int64_t>(0, std::min<int64_t>(100, pct)));
}

std::string build_k117(const json& raw) {
  const json& s = jget(raw, "settings");
  int mask = 0;
  if (ji(s, "adb_enabled")) mask |= 0x1;
  if (ji(s, "development_settings_enabled")) mask |= 0x2;
  // bit 0x4 = target (QQLive, release) debuggable -> 0.
  if (ji(s, "debugger_connected")) mask |= 0x8;
  return std::to_string(mask);
}

// K34 first-request runtime status. With a synthesized fresh K26 (no QQLive
// cache), this is the documented outside-app default; for byte-fidelity to a
// real install the "210" config + K26 load/save state come from root material.
std::string build_k34(const json& raw, const Material* material) {
  const json& dev = jget(raw, "device");
  const json& app = jget(raw, "app");
  int64_t uid = material && material->package_uid > 0
                    ? material->package_uid
                    : ji(app, "qqlive_uid");
  int64_t userid = material
                       ? material->android_user_id
                       : ji(app, "android_user_id");
  int64_t t = ji(dev, "current_time_ms");
  std::string cl = material && !material->k34_cl.empty()
                       ? material->k34_cl
                       : "0_0";
  std::string r = material && !material->k34_r.empty()
                      ? material->k34_r
                      : "0_1";
  int us503 = material ? material->k34_us_503 : 0;
  int us502 = material ? material->k34_us_502 : 0;
  std::string stif = material ? material->k34_stif : "";
  std::string tail =
      material && !material->k34_native_tail.empty()
          ? material->k34_native_tail
          : "EDB:2_0_0,NRF:-1_-1_-1_-1_-1,"
            "NSF:-1_-1_-1_-1_-1,UD:" + std::to_string(userid);
  std::string us = std::to_string(us503) + "_" + std::to_string(us502) +
                   "_" + std::to_string(uid);
  return "T:" + std::to_string(t) +
         ",LT:0,F:1,PT:1,PKG:1,SC:1,SRT:1,CL:" + cl + ",R:" + r +
         ",US:" + us + ",STIF:" + stif + ",GCI:2925a0b," + tail;
}

std::string build_k125(const json& raw) {
  std::string out;
  bool first = true;
  for (const auto& e : jget(raw, "path_probe")) {
    std::string path = js(e, "path");
    int64_t h = static_cast<int64_t>(murmur64a(path));  // signed %ld
    int state = ji(e, "writable") ? 3 : 1;
    int perm9 = static_cast<int>(ji(e, "mode")) & 0777;
    if (!first) out += ",";
    first = false;
    out += std::to_string(h) + ":" + std::to_string(state) + ":" +
           std::to_string(ji(e, "uid")) + "_" + std::to_string(ji(e, "dev")) + "_" +
           std::to_string(ji(e, "size")) + "_" + std::to_string(perm9) + "_" +
           std::to_string(ji(e, "ino"));
  }
  return out;
}

std::string build_k135(const json& raw) {
  std::string mounts = js(jget(raw, "files"), "proc_mounts");
  int loop = 0, dm = 0;
  size_t pos = 0;
  while (pos < mounts.size()) {
    size_t eol = mounts.find('\n', pos);
    std::string line = mounts.substr(pos, eol == std::string::npos ? eol : eol - pos);
    std::string dev = line.substr(0, line.find(' '));
    if (dev.rfind("/dev/block/loop", 0) == 0) loop++;
    else if (dev.rfind("/dev/block/dm", 0) == 0) dm++;
    if (eol == std::string::npos) break;
    pos = eol + 1;
  }
  return "/dev/block/loop::" + std::to_string(loop) + ",/dev/block/dm::" + std::to_string(dm);
}

std::string build_k141(const json& raw) {
  std::string mi = js(jget(raw, "files"), "proc_meminfo");
  auto p = mi.find("MemTotal:");
  if (p == std::string::npos) return "";
  p += 9;
  while (p < mi.size() && (mi[p] == ' ' || mi[p] == '\t')) p++;
  std::string num;
  while (p < mi.size() && mi[p] >= '0' && mi[p] <= '9') num += mi[p++];
  return num;
}

// K21/K108 environment bitmask.
const std::set<uint64_t> kBinHashes = {
    188486871556867391ULL, 4625056026677834780ULL, 5699186860183242502ULL,
    6375390942739310798ULL, 9571355056129943034ULL, 10176539077976802147ULL,
    11239373374017205912ULL, 12088468096961006599ULL, 12141382959481777401ULL,
    12261966668983626222ULL, 13842788585220729885ULL, 16740114592726295385ULL};
const std::set<uint64_t> kLibHashes = {
    3275220584839237549ULL, 5261928223529054707ULL, 7885936323246342029ULL,
    7919709837020428464ULL, 10191737423566771498ULL, 16106713152112522348ULL};
const char* kPropMarkers[] = {
    "init.svc.vbox86-setup", "init.svc.droid4x", "init.svc.qemud",
    "init.svc.su_kpbs_daemon", "init.svc.noxd", "init.svc.ttVM_x86-setup",
    "init.svc.xxkmsg", "init.svc.microvirtd", "microvirt.vbox_dpi",
    "microvirt.vbox_sdcard_drive", "nemud.battery.full", "init.svc.qemu-props"};

bool any_hash(const json& arr, const std::set<uint64_t>& set) {
  for (const auto& v : arr)
    if (v.is_string() && set.count(murmur64a(v.get<std::string>()))) return true;
  return false;
}

void build_k21_k108(const json& raw, std::string& k21, std::string& k108) {
  const json& props = jget(raw, "props");
  const json& env = jget(raw, "env");
  bool prop_hit = false;
  for (const char* k : kPropMarkers)
    if (!js(props, k).empty()) { prop_hit = true; break; }
  bool bin_hit = any_hash(jget(env, "system_bin_nondirs"), kBinHashes);
  int64_t sdk = ji(props, "ro.build.version.sdk");
  bool lib_hit = sdk >= 20 && any_hash(jget(env, "system_lib64_nondirs"), kLibHashes);
  if (!lib_hit) lib_hit = any_hash(jget(env, "system_lib_nondirs"), kLibHashes);

  bool thermal_bad = false;
  if (js(props, "ro.product.brand") != "HM 2A") {
    const json& th = jget(env, "thermal_entries");
    if (th.is_array() && !th.empty()) {
      bool has_zone = false; int temp_colon = 0;
      for (const auto& v : th) {
        std::string n = v.get<std::string>();
        if (n.find("zone") != std::string::npos) has_zone = true;
        if (n.find("temp:") != std::string::npos) temp_colon++;
      }
      thermal_bad = !has_zone && temp_colon < 5;
    }
  }
  bool usb_missing = !(sdk >= 1 && sdk < 11) && ji(env, "android_usb_exists") == 0;
  bool mount_marker =
      js(jget(raw, "files"), "proc_mounts").find("nonenonenone") != std::string::npos;

  int mask = 0;
  if (prop_hit) mask |= 0x01;
  if (bin_hit) mask |= 0x02;
  if (!lib_hit) mask |= 0x04;
  if (thermal_bad) mask |= 0x08;
  if (usb_missing) mask |= 0x10;
  if (mount_marker) mask |= 0x100;
  int score = prop_hit + bin_hit + (!lib_hit) + thermal_bad + usb_missing + mount_marker;
  k21 = score > 2 ? "1" : "0";
  k108 = std::to_string(mask);
}

}  // namespace

std::string synth_k26() {
  static std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<int> hex(0, 15);
  const char* H = "0123456789abcdef";
  std::string out;
  for (int i = 0; i < 32; i++) {
    int v = hex(rng);
    if (i == 12) v = 4;                 // UUID version 4
    if (i == 16) v = 8 + (v & 3);       // variant
    out += H[v];
  }
  return out;
}

uint64_t murmur64a(const void* data, std::size_t n, uint64_t seed) {
  const uint64_t M = 0xc6a4a7935bd1e995ULL;
  const int R = 47;
  const auto* d = static_cast<const uint8_t*>(data);
  uint64_t h = seed ^ (n * M);
  std::size_t end = n & ~7ULL;
  for (std::size_t i = 0; i < end; i += 8) {
    uint64_t k;
    std::memcpy(&k, d + i, 8);  // little-endian load
    k *= M; k ^= k >> R; k *= M;
    h ^= k; h *= M;
  }
  std::size_t tail = n & 7;
  const uint8_t* t = d + end;
  if (tail >= 7) h ^= (uint64_t)t[6] << 48;
  if (tail >= 6) h ^= (uint64_t)t[5] << 40;
  if (tail >= 5) h ^= (uint64_t)t[4] << 32;
  if (tail >= 4) h ^= (uint64_t)t[3] << 24;
  if (tail >= 3) h ^= (uint64_t)t[2] << 16;
  if (tail >= 2) h ^= (uint64_t)t[1] << 8;
  if (tail >= 1) { h ^= (uint64_t)t[0]; h *= M; }
  h ^= h >> R; h *= M; h ^= h >> R;
  return h;
}

uint64_t simhash64(const std::vector<uint64_t>& hashes) {
  uint64_t out = 0;
  for (int bit = 0; bit < 64; bit++) {
    int score = 0;
    uint64_t mask = 1ULL << bit;
    for (uint64_t v : hashes) score += (v & mask) ? 1 : -1;
    if (score > 0) out |= mask;
  }
  return out;
}

std::map<int, std::string> build_k_values(const json& raw,
                                          const Material* material) {
  const json& props = jget(raw, "props");
  const json& dev = jget(raw, "device");
  std::map<int, std::string> k;
  auto P = [&](const char* key) { return js(props, key); };

  // device properties
  k[8] = P("ro.build.fingerprint");
  k[23] = P("ro.product.brand");
  k[24] = P("ro.product.model");
  k[25] = P("ro.build.version.release");
  k[33] = P("ro.build.version.sdk");
  k[42] = P("ro.product.manufacturer");
  k[51] = !P("ro.board.platform").empty() ? P("ro.board.platform") : P("ro.hardware");
  k[35] = P("ro.runtime.firstboot");

  // raw-fact builders
  k[9] = build_k9(raw);
  k[10] = build_k10(raw);
  k[12] = build_k12(raw);
  k[13] = js(dev, "battery_capacity");
  {
    int64_t w = ji(dev, "screen_width"), h = ji(dev, "screen_height");
    int64_t lo = std::min(w, h), hi = std::max(w, h);
    k[14] = std::to_string(lo) + "*" + std::to_string(hi);
  }
  k[15] = js(dev, "ringtone_display_name");
  k[11] = build_k11(raw);
  k[16] = build_k16(raw);
  k[17] = build_k17(raw);
  k[18] = build_k18(raw);
  k[39] = build_k39(raw);
  k[105] = build_k105(raw);
  k[125] = build_k125(raw);
  k[126] = std::to_string(ji(dev, "current_time_ms") - ji(dev, "elapsed_realtime_ms"));
  k[135] = build_k135(raw);
  k[141] = build_k141(raw);
  k[148] = js(jget(raw, "files"), "boot_id");
  k[20] = ji(jget(raw, "env"), "su_accessible") ? "4" : "0";
  k[117] = build_k117(raw);
  build_k21_k108(raw, k[21], k[108]);

  // K26: device UUID anchor. Not readable by the non-root probe (QQLive private
  // cache), so synthesize a fresh one (native UUID.randomUUID fallback). The
  // server requires K26 non-empty to mint; reuse the same value for a stable
  // device identity. K34 follows from a fresh K26.
  k[26] = material && !material->k26_wire.empty()
              ? material->k26_wire
              : synth_k26();
  k[34] = build_k34(raw, material);

  // constants
  k[37] = "268435456";
  k[46] = "0";
  k[101] = "0"; k[102] = "0"; k[103] = "0"; k[104] = "1";
  k[107] = "com.tencent.qqlive_106409A8F91A970D58BEB2263CE7550F";
  k[113] = "0"; k[114] = "0";
  k[118] = "v4;-1_v6;-1";
  k[138] = "5,1";
  k[200] = "32,2,3,4,36,5,6,40,136,43,143,144,10001,10002,19,10003,99905729";

  // empty / config-server / app-private
  for (int e : {22, 26, 28, 29, 34, 44, 45, 100, 106, 115, 116, 117, 122})
    if (!k.count(e)) k[e] = "";

  return k;
}

}  // namespace tencent::turing
