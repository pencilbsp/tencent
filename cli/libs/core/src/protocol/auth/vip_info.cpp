// vipinfo.cpp — Port byte-exact ../analytics/app/vipinfo_response.py + vipinfo_client.py
// (build_vip_request_body, _response_error). Xem vipinfo.h.
#include "tencent/core/protocol/auth/vip_info.h"

#include <cstdio>
#include <ctime>

#include "tencent/core/codec/protobuf.h"

namespace tencent::vip {

namespace pb = tencent::pb;

// ../analytics/app/vipinfo_client.py:53-58 — VipUserInfoRequest{ field 2 repeated varint }.
std::string build_vip_request_body(const std::vector<int>& vip_types) {
  std::string out;
  for (int t : vip_types) {
    out.push_back('\x10');  // tag 0x10 = field 2, wire 0 (varint)
    out += pb::write_varint(static_cast<uint64_t>(t));
  }
  return out;
}

// ── parse ────────────────────────────────────────────────────────────────────
static int64_t i_of(const pb::Field* f) {
  return f ? static_cast<int64_t>(f->intval) : 0;  // ../analytics/app/vipinfo_response.py:37
}

// ../analytics/app/vipinfo_response.py:41-48
static VipBase parse_vip_base(const std::string& buf) {
  std::vector<pb::Field> f = pb::parse(buf);
  VipBase b;
  b.is_vip = i_of(pb::get_first(f, 1)) != 0;
  b.begin_time = i_of(pb::get_first(f, 2));
  b.end_time = i_of(pb::get_first(f, 3));
  b.level = static_cast<int>(i_of(pb::get_first(f, 4)));
  return b;
}

// ../analytics/app/vipinfo_response.py:51-63
static VipExtra parse_vip_extra(const std::string& buf) {
  std::vector<pb::Field> f = pb::parse(buf);
  VipExtra e;
  const pb::Field* end_msg = pb::get_first(f, 1);
  if (end_msg && end_msg->wire == 2) e.end_msg = end_msg->data;
  const pb::Field* icon = pb::get_first(f, 3);
  if (icon && icon->wire == 2 && icon->data.rfind("http", 0) == 0) e.icon_url = icon->data;
  return e;
}

// ../analytics/app/vipinfo_response.py:66-75
static VipUserInfo parse_vip_user_info(const std::string& buf) {
  std::vector<pb::Field> f = pb::parse(buf);
  VipUserInfo out;
  const pb::Field* base = pb::get_first(f, 1);
  if (base) { out.has_base = true; out.base = parse_vip_base(base->data); }
  const pb::Field* extra = pb::get_first(f, 2);
  if (extra) { out.has_extra = true; out.extra = parse_vip_extra(extra->data); }
  return out;
}

// ../analytics/app/vipinfo_response.py:78-104
VipResponse parse_vip_response(const std::string& body) {
  std::vector<pb::Field> f = pb::parse(body);
  VipResponse res;
  const pb::Field* hw = pb::get_first(f, 1);
  if (hw) { res.has_hollywood = true; res.hollywood = parse_vip_user_info(hw->data); }
  const pb::Field* nba = pb::get_first(f, 2);
  if (nba && !nba->data.empty()) { res.has_nba = true; res.nba = parse_vip_user_info(nba->data); }
  const pb::Field* vvip = pb::get_first(f, 4);
  if (vvip && !vvip->data.empty()) { res.has_vvip = true; res.vvip = parse_vip_user_info(vvip->data); }
  const pb::Field* jd = pb::get_first(f, 3);
  if (jd) { res.has_json = true; res.json_data = jd->data; }
  // map<int32,VipUserInfo>: mỗi f5 = { f1=key(varint), f2=VipUserInfo }.
  for (const pb::Field* entry : pb::get_all(f, 5)) {
    std::vector<pb::Field> sub = pb::parse(entry->data);
    const pb::Field* k = pb::get_first(sub, 1);
    const pb::Field* v = pb::get_first(sub, 2);
    if (k) res.by_type[static_cast<int>(k->intval)] = v ? parse_vip_user_info(v->data) : VipUserInfo{};
  }
  return res;
}

// ── summarize ────────────────────────────────────────────────────────────────
static const char* py_bool(bool b) { return b ? "True" : "False"; }

// Python `//` (floor division), khác trunc của C++ khi tử số âm.
static int64_t floordiv(int64_t a, int64_t b) {
  int64_t q = a / b, r = a % b;
  if (r != 0 && ((r < 0) != (b < 0))) --q;
  return q;
}

// ../analytics/app/vipinfo_response.py:107-110 — UTC+8, "%Y-%m-%d %H:%M (UTC+8)".
static std::string fmt_ts(int64_t ts) {
  if (ts == 0) return "-";
  std::time_t t = static_cast<std::time_t>(ts + 8 * 3600);
  std::tm g{};
#if defined(_WIN32)
  gmtime_s(&g, &t);
#else
  gmtime_r(&t, &g);
#endif
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d (UTC+8)",
                g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min);
  return buf;
}

// VIP_TYPE_NAMES (../analytics/app/vipinfo_response.py:32-34).
static const char* vip_type_name(int t) {
  switch (t) {
    case 3: return "video_svip";
    case 3006: return "sport_vip";
    case 3007: return "skp";
    case 3012: return "family";
    case 3013: return "kids/other";
    default: return "?";
  }
}

// ../analytics/app/vipinfo_response.py:113-133
std::string summarize(const VipResponse& resp, int64_t now_ts) {
  std::vector<std::string> lines;
  char buf[256];

  if (resp.has_hollywood && resp.hollywood.has_base) {
    const VipBase& hw = resp.hollywood.base;
    int64_t days = hw.end_time ? floordiv(hw.end_time - hw.begin_time, 86400) : 0;
    int64_t remain = hw.end_time ? floordiv(hw.end_time - now_ts, 86400) : 0;
    lines.push_back("=== Video VIP (hollywood) ===");
    std::snprintf(buf, sizeof(buf), "  is_vip   : %s", py_bool(hw.is_vip));
    lines.push_back(buf);
    std::snprintf(buf, sizeof(buf), "  level    : %d  (SVIP%d)", hw.level, hw.level);
    lines.push_back(buf);
    lines.push_back("  begin    : " + fmt_ts(hw.begin_time));
    lines.push_back("  end      : " + fmt_ts(hw.end_time) + "   <- VIP EXPIRED");
    std::snprintf(buf, sizeof(buf), "  duration : %lld days, ~%lld days remaining",
                  static_cast<long long>(days), static_cast<long long>(remain));
    lines.push_back(buf);
    if (resp.hollywood.has_extra && !resp.hollywood.extra.icon_url.empty())
      lines.push_back("  icon     : " + resp.hollywood.extra.icon_url);
  }

  for (const auto& [t, info] : resp.by_type) {  // std::map -> sort theo key
    if (info.has_base && info.base.is_vip) {
      std::snprintf(buf, sizeof(buf), "  type %d (%s): is_vip=%s level=%d end=",
                    t, vip_type_name(t), py_bool(info.base.is_vip), info.base.level);
      lines.push_back(std::string(buf) + fmt_ts(info.base.end_time));
    }
  }

  std::string out;
  for (std::size_t i = 0; i < lines.size(); ++i) {
    if (i) out += "\n";
    out += lines[i];
  }
  return out;
}

// ../analytics/app/vipinfo_client.py:81-82 — _signed64 = diễn giải two's-complement 64-bit.
static int64_t signed64(uint64_t v) { return static_cast<int64_t>(v); }

// ── response_error (../analytics/app/vipinfo_client.py:85-95) ──────────────────────────
std::string response_error(const std::string& head) {
  std::vector<pb::Field> fields = pb::parse(head);
  const pb::Field* ret = pb::get_first(fields, 5);
  const pb::Field* msg = pb::get_first(fields, 6);
  if (!ret && !msg) return "";
  int64_t ret_s = ret ? signed64(ret->intval) : 0;
  std::string msg_s = (msg && msg->wire == 2) ? msg->data : "";
  if (ret_s != 0 || !msg_s.empty()) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "ret=%lld msg=", static_cast<long long>(ret_s));
    return std::string(buf) + (msg_s.empty() ? "(empty)" : msg_s);
  }
  return "";
}

}  // namespace tencent::vip
