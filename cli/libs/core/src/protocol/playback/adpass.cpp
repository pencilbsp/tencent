// adpass.cpp — Xem adpass.h.
#include "tencent/core/protocol/playback/adpass.h"

#include <iomanip>
#include <sstream>

namespace tencent::playback {

namespace {

std::string json_string(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 2);
  out.push_back('"');
  static const char* HEX = "0123456789ABCDEF";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          out.push_back(HEX[c >> 4]);
          out.push_back(HEX[c & 0x0F]);
        } else {
          out.push_back(char(c));
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

void add_kv(std::string& out, const std::string& key, const std::string& value) {
  if (out.back() != '{') out.push_back(',');
  out += json_string(key);
  out.push_back(':');
  out += json_string(value);
}

void add_bool(std::string& out, const std::string& key, bool value) {
  if (out.back() != '{') out.push_back(',');
  out += json_string(key);
  out.push_back(':');
  out += value ? "true" : "false";
}

void add_int(std::string& out, const std::string& key, int value) {
  if (out.back() != '{') out.push_back(',');
  out += json_string(key);
  out.push_back(':');
  out += std::to_string(value);
}

}  // namespace

std::string java_urlencode(const std::string& s) {
  static const std::string keep = "_.-~*";
  static const char* HEX = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        keep.find(char(c)) != std::string::npos) {
      out.push_back(char(c));
    } else if (c == ' ') {
      out.push_back('+');
    } else {
      out.push_back('%');
      out.push_back(HEX[c >> 4]);
      out.push_back(HEX[c & 0x0F]);
    }
  }
  return out;
}

std::string build_qad_adpass_json(const QadAdpassOptions& opt) {
  std::string out = "{";
  add_bool(out, "shouldSkipCustomAdWithoutLogin", false);
  add_kv(out, "interestAdTags", "");
  add_kv(out, "adCookie", opt.ad_cookie);
  add_kv(out, "taidTicket", opt.taid_ticket);
  add_kv(out, "ad_qq_openid", "");
  add_kv(out, "mnt_id", "");
  add_kv(out, "device_init_time", "");
  add_kv(out, "ad_experiment_id", "0");
  add_kv(out, "encryptedOaid", "");
  add_bool(out, "isPreviewMode", false);
  add_kv(out, "skp_mode", "0");
  add_kv(out, "is_blind_authorize", "0");
  add_int(out, "wxVersionCode", opt.wx_version_code);
  add_kv(out, "osVersion", opt.os_version);
  add_bool(out, "is_free_mode_user", false);
  add_bool(out, "free_mode_user_has_permission", false);
  out.push_back('}');
  return out;
}

std::string build_qad_adpass(const QadAdpassOptions& opt) {
  return java_urlencode(build_qad_adpass_json(opt));
}

}  // namespace tencent::playback
