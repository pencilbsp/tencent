// test_login_sign.cpp — Test byte-exact cho login-refresh vusession.
//
// So login_sign (energy/sign/nonce) + login_refresh (body/frame/response) với
// ground-truth Python trong generated_vectors_sign.h. Xem AGENTS.md §0.
#include <cstdio>
#include <string>

#include "vectors/generated_vectors_sign.h"
#include "tencent/core/protocol/auth/login_refresh.h"
#include "tencent/core/protocol/auth/login_sign.h"
#include "tencent/core/protocol/trpc/head.h"

namespace ls = tencent::auth;
namespace lr = tencent::auth;

static int g_failures = 0;

static std::string from_hex(const std::string& hex) {
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<char>((nib(hex[i]) << 4) | nib(hex[i + 1])));
  return out;
}
static std::string to_hex(const std::string& b) {
  static const char* d = "0123456789abcdef";
  std::string out;
  for (unsigned char c : b) { out.push_back(d[c >> 4]); out.push_back(d[c & 0xF]); }
  return out;
}
static void check_hex(const std::string& got_bytes, const std::string& want_hex,
                      const std::string& name) {
  std::string got = to_hex(got_bytes);
  bool ok = got == want_hex;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) {
    ++g_failures;
    std::printf("    want: %s\n    got : %s\n", want_hex.c_str(), got.c_str());
  }
}
static void check_str(const std::string& got, const std::string& want, const std::string& name) {
  bool ok = got == want;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) {
    ++g_failures;
    std::printf("    want: %s\n    got : %s\n", want.c_str(), got.c_str());
  }
}
static void check_i64(int64_t got, int64_t want, const std::string& name) {
  bool ok = got == want;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) { ++g_failures; std::printf("    want: %lld\n    got : %lld\n",
                                       (long long)want, (long long)got); }
}

int main() {
  std::printf("== login-refresh vusession ==\n");

  // (1) nonce derivation.
  for (const auto& c : vsign::NONCE_CASES)
    check_hex(ls::derive_nonce(c.usec), c.nonce_hex,
              "derive_nonce(usec=" + std::to_string(c.usec) + ")");

  // (1) energy + sign (usec inject).
  for (const auto& c : vsign::ENERGY_CASES) {
    std::string msg = from_hex(c.msg_hex);
    std::string tag = "energy(len=" + std::to_string(msg.size()) +
                      ", usec=" + std::to_string(c.usec) + ")";
    check_hex(ls::energy(msg, c.usec), c.energy_hex, tag);
    check_str(ls::make_sign(msg, c.usec), c.sign, "sign " + tag);
  }

  // (2) build_original_data + build_refresh_request body.
  check_str(ls::build_original_data(vsign::GUID, vsign::VUREFRESH, vsign::NOW_MS, vsign::RND),
            vsign::ORIGINAL_DATA, "build_original_data");
  std::string body = lr::build_refresh_request(
      vsign::VUID, vsign::VUSESSION, vsign::VUREFRESH, vsign::WX_APP_ID,
      vsign::WX_OPEN_ID, vsign::WX_ACCESS_TOKEN, vsign::WX_REFRESH_TOKEN,
      vsign::QIMEI36, vsign::ORIGINAL_DATA, vsign::SIGN);
  check_hex(body, vsign::REFRESH_BODY_HEX, "build_refresh_request body");

  // (3) Refresh frame từ session có đầy đủ device/app context.
  tencent::model::SessionMaterial base;
  base.kv["vuid"] = std::to_string(vsign::VUID);
  base.kv["vusession"] = vsign::VUSESSION;
  base.kv["vurefresh"] = vsign::VUREFRESH;
  base.kv["wx_app_id"] = vsign::WX_APP_ID;
  base.kv["wx_open_id"] = vsign::WX_OPEN_ID;
  base.kv["wx_access_token"] = vsign::WX_ACCESS_TOKEN;
  base.kv["wx_refresh_token"] = vsign::WX_REFRESH_TOKEN;
  base.kv["qimei36"] = vsign::QIMEI36;
  base.kv["guid"] = vsign::GUID;
  base.kv["usid"] = vsign::USID;
  base.kv["model"] = "SM-G991N";
  base.kv["brand"] = "samsung";
  base.kv["os_version"] = "14";
  base.kv["android_api_level"] = "34";
  base.kv["app_ver"] = "9.02.90.31109";
  base.kv["app_build"] = "31109";
  base.kv["app_sub_build"] = "20563";
  base.kv["platform"] = "1000005";
  base.kv["screen_width"] = "1080";
  base.kv["screen_height"] = "2176";
  base.kv["screen_dpi"] = "480";
  std::string frame2 = lr::build_refresh_frame_from_session(base, vsign::NOW_MS, vsign::RND,
                                                            vsign::USEC, /*sign=*/"");
  check_hex(frame2, vsign::REFRESH_FRAME_HEX, "build_refresh_frame_from_session");

  // (4) parse RefreshResponse.
  lr::RefreshResult r = lr::parse_refresh_response(from_hex(vsign::RESP_FRAME_HEX));
  check_i64(r.vuid, vsign::RESP_VUID, "resp.vuid");
  check_str(r.vusession, vsign::RESP_VUSESSION, "resp.vusession");
  check_i64(r.vusession_expire_timestamp, vsign::RESP_EXPIRE_TS, "resp.expire_ts");
  check_str(r.vurefresh, vsign::RESP_VUREFRESH, "resp.vurefresh");
  check_str(r.third_access_token, vsign::RESP_ACCESS_TOKEN, "resp.access_token");
  check_str(r.third_refresh_token, vsign::RESP_REFRESH_TOKEN, "resp.refresh_token");
  check_i64(r.trpc_ret, vsign::RESP_TRPC_RET, "resp.trpc_ret");
  check_str(r.has_trpc_ret ? "yes" : "no", "yes", "resp.has_trpc_ret");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
