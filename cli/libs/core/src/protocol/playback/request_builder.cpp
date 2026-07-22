// getvinfo_params.cpp — Xem getvinfo_params.h. Port từ ../analytics/app/getvinfo_params.py.
#include "tencent/core/protocol/playback/request_builder.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <unordered_map>

#include "tencent/core/protocol/playback/adpass.h"
#include "tencent/core/protocol/playback/ckey_fields.h"
#include "tencent/core/protocol/playback/constants.h"
#include "tencent/core/common/hex.h"

namespace tencent::playback {

namespace {

// ── OrderedParams: map giữ thứ tự với ngữ nghĩa Python dict ───────────────────
// set(k,v): có key -> cập nhật giữ vị trí; không -> append (dict[k]=v).
// erase(k): bỏ key (dict.pop(k, None)).
class OrderedParams {
 public:
  OrderedParams() = default;
  explicit OrderedParams(const OrderedKV& kv) {
    for (const auto& p : kv) set(p.first, p.second);
  }
  void set(const std::string& k, const std::string& v) {
    auto it = idx_.find(k);
    if (it != idx_.end()) {
      items_[it->second].second = v;
    } else {
      idx_[k] = items_.size();
      items_.emplace_back(k, v);
    }
  }
  void erase(const std::string& k) {
    auto it = idx_.find(k);
    if (it == idx_.end()) return;
    size_t pos = it->second;
    items_.erase(items_.begin() + pos);
    idx_.erase(it);
    for (auto& kv : idx_)
      if (kv.second > pos) --kv.second;
  }
  const std::string* get(const std::string& k) const {
    auto it = idx_.find(k);
    return it == idx_.end() ? nullptr : &items_[it->second].second;
  }
  const OrderedKV& items() const { return items_; }

 private:
  OrderedKV items_;
  std::unordered_map<std::string, size_t> idx_;
};

// b[:target].ljust(target, b"0") — getvinfo_params.py:147-148/152-153.
Bytes adjust_len(const Bytes& b, size_t target) {
  Bytes out(b.begin(), b.begin() + std::min(b.size(), target));
  while (out.size() < target) out.push_back('0');  // 0x30
  return out;
}

std::string bytes_to_ascii(const Bytes& b) { return std::string(b.begin(), b.end()); }

std::string pick_context_value(const GetvinfoContext& context, const std::string& key,
                               const std::string& fallback = "") {
  const std::string* v = context.find_param(key);
  return (v != nullptr && !v->empty()) ? *v : fallback;
}

std::string adpass_os_version(const OrderedParams& out, const BuildOptions& opt) {
  if (opt.adpass_os_version.has_value() && !opt.adpass_os_version->empty())
    return *opt.adpass_os_version;
  const std::string* osver = out.get("osver");
  std::string value = (osver != nullptr && !osver->empty()) ? *osver : std::string("14");
  return value.rfind("Android", 0) == 0 ? value : "Android_" + value;
}

uint64_t parse_timestamp_ms13(const Bytes& param2) {
  if (param2.size() < 13) throw std::invalid_argument("Field[20] param2 shorter than 13");
  uint64_t out = 0;
  for (int i = 0; i < 13; ++i) {
    if (param2[i] < '0' || param2[i] > '9')
      throw std::invalid_argument("Field[20] param2 must start with timestamp_ms");
    out = out * 10 + uint64_t(param2[i] - '0');
  }
  return out;
}

uint64_t now_us() {
  using namespace std::chrono;
  return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}
Bytes rand_bytes(size_t n) {
  std::random_device rd;
  Bytes out(n);
  for (auto& b : out) b = uint8_t(rd() & 0xFF);
  return out;
}

// Current native captures keep preamble[0..19] stable and store cKey creation
// epoch seconds as BE u32 at [20..23]. A stale captured word is rejected with
// em=85/exem=-3. Source: live differential request.txt/request.today.txt and
// ../analytics/app/getvinfo_params.py::patch_preamble_epoch.
void patch_preamble_epoch(Bytes& preamble, uint64_t timestamp_ms) {
  if (preamble.size() != 24)
    throw std::invalid_argument("cKey preamble must be 24 bytes");
  uint32_t epoch_seconds = uint32_t((timestamp_ms / 1000ULL) & 0xFFFF'FFFFULL);
  preamble[20] = uint8_t(epoch_seconds >> 24);
  preamble[21] = uint8_t(epoch_seconds >> 16);
  preamble[22] = uint8_t(epoch_seconds >> 8);
  preamble[23] = uint8_t(epoch_seconds);
}

// uuid4 string (chỉ dùng runtime, không test byte-exact).
std::string uuid4() {
  Bytes b = rand_bytes(16);
  b[6] = uint8_t((b[6] & 0x0F) | 0x40);  // version 4
  b[8] = uint8_t((b[8] & 0x3F) | 0x80);  // variant
  std::string h = util::hex_encode(b);
  return h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) + "-" +
         h.substr(16, 4) + "-" + h.substr(20, 12);
}

}  // namespace

// ── flowid (getvinfo_params.py:81-91) ────────────────────────────────────────

std::string make_flowid(const std::string& platform, const std::string& uuid_str,
                        uint64_t nano) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%014llu", static_cast<unsigned long long>(nano));
  return uuid_str + buf + "_" + platform;
}

std::string make_flowid_random(const std::string& platform) {
  using namespace std::chrono;
  uint64_t nano = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
  nano %= 100000000000000ULL;  // % 10^14 (getvinfo_params.py:90)
  return make_flowid(platform, uuid4(), nano);
}

// ── cKey generation (getvinfo_params.py:109-170) ─────────────────────────────

std::string generate_ckey(const GetvinfoContext& context, const std::string& vid,
                          bool bind_vid,
                          bool keep_template_param2, const CkeyEntropy& ent,
                          const std::string& component2, const std::string& qimei36) {
  Bytes xor_key = context.xor_key();
  Bytes tea_key = context.tea_key();
  std::string app_ver = pick_context_value(context, "appVer", constants::kAppVersion);
  std::string guid = pick_context_value(context, "guid", context.guid);
  std::string qimei = pick_context_value(context, "qimei", context.qimei36);
  std::string device_model = context.device_model;
  if (device_model.empty()) device_model = pick_context_value(context, "devicemodel");
  if (device_model.empty())
    throw std::invalid_argument("missing account device model for cKey");
  std::vector<Bytes> fields = build_ckey_base_fields(app_ver, guid, qimei, device_model);
  Bytes preamble = context.preamble();
  Bytes trailing = context.trailing();
  uint32_t raw_bitmap = context.raw_bitmap;
  std::vector<uint32_t> raw_ints = context.raw_ints;

  if (!keep_template_param2) {
    uint64_t timestamp_ms = parse_timestamp_ms13(ent.param2);
    uint64_t epoch_seconds = timestamp_ms / 1000ULL;
    patch_preamble_epoch(preamble, timestamp_ms);

    // Field[15] native playback JSON — solved offline, no template copy.
    if (fields.size() > 14) fields[14] = build_field15_json_native();
    // Field[20] param2 (index 19) — căn về độ dài field gốc.
    if (fields.size() > 19) {
      fields[19] = adjust_len(ent.param2, fields[19].size());
      // Field[16].sd mirrors Field[20]; Field[16].dd = urandom8 + ts_ms + crc tail.
      if (fields.size() > 15) {
        std::string sd = bytes_to_ascii(fields[19]);
        std::string dd = build_field16_dd(timestamp_ms, ent.field16_dd_random8);
        fields[15] = build_field16_crypto_algo_json(sd, dd);
      }
    }
    // Field[21] rand (index 20).
    if (fields.size() > 20) fields[20] = adjust_len(ent.rand21, fields[20].size());

    // Field[19] full native chain — MD5(epoch, component2, qimei36) + ARX/check-tail.
    if (fields.size() > 18) {
      std::string f19_component2 = component2.empty() ? vid : component2;
      std::string f19_qimei36 = qimei36.empty() ? context.qimei36 : qimei36;
      if (!f19_component2.empty() && !f19_qimei36.empty())
        fields[18] = derive_field19_from_components(epoch_seconds, f19_component2,
                                                     f19_qimei36, ent.tv_usec);
    }
  }
  // Field[4] (index 3) = vid — getvinfo_params.py:164-165.
  if (bind_vid && fields.size() >= 4 && !vid.empty())
    fields[3] = Bytes(vid.begin(), vid.end());

  return encode_ckey(fields, preamble, tea_key, xor_key, raw_bitmap, raw_ints, trailing,
                     ent.pad);
}

std::string generate_ckey_random(const GetvinfoContext& context, const std::string& vid,
                                 bool bind_vid, const std::string& component2,
                                 const std::string& qimei36) {
  // fresh_param2_and_random (getvinfo_params.py:99-106).
  CkeyEntropy e;
  uint64_t sampled_us = now_us();
  std::string p2 = std::to_string(sampled_us / 1000ULL) + util::hex_encode(rand_bytes(8));
  e.param2 = Bytes(p2.begin(), p2.end());
  std::string r21 = util::hex_encode(rand_bytes(8));
  e.rand21 = Bytes(r21.begin(), r21.end());
  e.field16_dd_random8 = rand_bytes(8);
  e.tv_usec = uint32_t(sampled_us % 1000000ULL);
  e.pad = random_pad_entropy();
  return generate_ckey(context, vid, bind_vid, /*keep_template_param2=*/false, e,
                       component2, qimei36);
}

// ── Body encoding (getvinfo_params.py:279-291) ───────────────────────────────

std::string url_quote(const std::string& s) {
  // urllib.parse.quote(s, safe="-_.*"): never-quote = ALNUM + always-safe
  // "_.-~" + safe "-_.*" => alnum + "_.-~*". %XX HOA cho phần còn lại.
  static const std::string keep = "_.-~*";
  static const char* HEX = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        keep.find(char(c)) != std::string::npos) {
      out.push_back(char(c));
    } else {
      out.push_back('%');
      out.push_back(HEX[c >> 4]);
      out.push_back(HEX[c & 0xF]);
    }
  }
  return out;
}

std::string encode_body(const OrderedKV& params) {
  std::string out;
  bool first = true;
  for (const auto& kv : params) {
    if (!first) out.push_back('&');
    first = false;
    out += kv.first;  // key KHÔNG encode (getvinfo_params.py:290)
    out.push_back('=');
    out += url_quote(kv.second);
  }
  return out;
}

// ── build_query_params (getvinfo_params.py:185-273) ──────────────────────────

OrderedKV build_query_params(const GetvinfoContext& context, const BuildOptions& opt) {
  if (opt.mode != "normal" && opt.mode != "history")
    throw std::invalid_argument("mode must be 'normal' or 'history'");

  // 1. Base = context.query_params (giữ thứ tự).
  OrderedParams out(context.query_params);

  // 2. Ép hằng số tĩnh (getvinfo_params.py:214).
  for (const auto& kv : constants::safe_static_constants()) out.set(kv.first, kv.second);

  // 3. Re-apply device/session keys từ context (getvinfo_params.py:177-220).
  //    TEMPLATE_OVERLAY_KEEP_KEYS = DEVICE + SESSION + (cid,newnettype,ipstack).
  static const std::vector<std::string> kKeepKeys = {
      "guid", "qimei", "abuserid", "userid", "openid", "viptype", "user_level",
      "detail_user_level", "rec_user_level", "cid", "newnettype", "ipstack",
      "platform", "sdtfrom", "appVer", "manufacturer", "osver", "screeninfo"};
  for (const auto& k : kKeepKeys) {
    const std::string* v = context.find_param(k);
    if (v != nullptr) out.set(k, *v);
  }
  if (!context.device_model.empty()) out.set("devicemodel", context.device_model);

  // QAD/adpass state is rebuilt, not copied from context query_params.
  if (opt.adpass.has_value()) {
    out.set("adpass", *opt.adpass);
  } else if (opt.include_adpass) {
    QadAdpassOptions ad;
    ad.ad_cookie = opt.adpass_cookie.value_or(context.cookies);
    ad.taid_ticket = opt.taid_ticket.value_or("");
    ad.os_version = adpass_os_version(out, opt);
    ad.wx_version_code = opt.adpass_wx_version_code.value_or(671106370);
    out.set("adpass", build_qad_adpass(ad));
  } else {
    out.erase("adpass");
  }

  // 4. PER_REQUEST_RUNTIME (getvinfo_params.py:223-232).
  for (const auto& kv : constants::per_request_runtime()) {
    if (kv.second.has_value())
      out.set(kv.first, *kv.second);
    else {
      const std::string* v = context.find_param(kv.first);
      if (v != nullptr) out.set(kv.first, *v);
    }
  }
  if (opt.bitrate.has_value()) out.set("bitrate", *opt.bitrate);
  if (opt.atime.has_value()) out.set("atime", *opt.atime);

  // 5. Path mode (getvinfo_params.py:235-248).
  if (opt.mode == "normal") {
    for (const auto& kv : constants::normal_path_additions()) out.set(kv.first, kv.second);
    out.set("vid", opt.vid);
    out.erase("previd");
    out.erase("previd_type");
  } else {  // history
    for (const auto& kv : constants::normal_path_additions()) out.erase(kv.first);
    for (const auto& kv : constants::history_path_required()) out.set(kv.first, kv.second);
    out.set("vid", "");
    if (opt.previd_json.has_value())
      out.set("previd", *opt.previd_json);
    else if (context.find_param("previd") == nullptr)
      throw std::invalid_argument("history mode requires previd_json or context previd");
  }

  // 6. User input overrides (getvinfo_params.py:251-254).
  if (opt.cid.has_value()) out.set("cid", *opt.cid);
  out.set("defn", opt.defn);
  out.set("scene", opt.scene);

  // 7. Per-request generated (getvinfo_params.py:257-262).
  std::string platform = out.get("platform") ? *out.get("platform") : std::string("10303");
  std::string flowid = (opt.flowid.has_value() && !opt.flowid->empty())
                           ? *opt.flowid
                           : make_flowid_random(platform);
  out.set("flowid", flowid);

  std::string ckey;
  if (opt.ckey.has_value() && !opt.ckey->empty()) {
    ckey = *opt.ckey;
  } else {
    const std::string* ov = out.get("vid");
    std::string ckey_vid = (ov != nullptr && !ov->empty()) ? *ov : opt.vid;
    std::string component2 = opt.ckey_component2.value_or(ckey_vid);
    const std::string* qimei = out.get("qimei");
    std::string qimei36 =
        (qimei != nullptr && !qimei->empty()) ? *qimei : context.qimei36;
    ckey = generate_ckey_random(context, ckey_vid, opt.ckey_bind_vid, component2,
                                qimei36);
  }
  out.set("cKey", ckey);

  // 8. Caller extra overrides (getvinfo_params.py:266-271). nullopt = xóa key.
  for (const auto& kv : opt.extra) {
    if (kv.second.has_value())
      out.set(kv.first, *kv.second);
    else
      out.erase(kv.first);
  }

  return out.items();
}

// ── Headers + full request (getvinfo_params.py:294-323) ──────────────────────

OrderedKV build_headers(const GetvinfoContext& context) {
  OrderedKV h = {
      {"Host", "vv6.video.qq.com"},
      {"Accept", "*/*"},
      {"User-Agent", "qqlive"},
      {"Accept-Encoding", "gzip"},
      {"Content-Type", "application/x-www-form-urlencoded"},
  };
  if (!context.cookies.empty()) h.emplace_back("Cookie", context.cookies);
  return h;
}

FullRequest build_full_request(const GetvinfoContext& context, const BuildOptions& opt) {
  FullRequest r;
  r.params = build_query_params(context, opt);
  r.body = encode_body(r.params);
  r.headers = build_headers(context);
  return r;
}

}  // namespace tencent::playback
