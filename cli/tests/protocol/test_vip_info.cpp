// test_vipinfo.cpp — Test byte-exact cho vipinfo (Pha C).
//
// So build_vip_request_body / parse_vip_response+summarize / response_error với
// ground-truth Python (../analytics/app/vipinfo_response.py + vipinfo_client.py) trong
// generated_vectors_acct.h. summarize dùng now_ts cố định (VIP_NOW_TS). AGENTS.md §0.
#include <cstdio>
#include <string>

#include "vectors/generated_vectors_acct.h"
#include "tencent/core/protocol/auth/vip_info.h"

namespace vip = tencent::vip;

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
static void check_eq(const std::string& got, const std::string& want, const char* name) {
  bool ok = got == want;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) {
    ++g_failures;
    std::printf("    want: %s\n", want.c_str());
    std::printf("    got : %s\n", got.c_str());
  }
}

int main() {
  std::printf("== Pha C: vipinfo ==\n");

  // request body [3,3006,3007,3013,3012].
  check_eq(to_hex(vip::build_vip_request_body(vip::kDefaultVipTypes)),
           vacc::VIP_REQ_BODY_HEX, "build_vip_request_body (default types)");

  // parse + summarize (now_ts cố định).
  vip::VipResponse resp = vip::parse_vip_response(from_hex(vacc::VIP_RESP_BODY_HEX));
  check_eq(vip::summarize(resp, vacc::VIP_NOW_TS), vacc::VIP_SUMMARY,
           "parse_vip_response + summarize");

  // vài kiểm tra cấu trúc.
  std::printf("  [%s] hollywood present + is_vip + level=3\n",
              (resp.has_hollywood && resp.hollywood.has_base && resp.hollywood.base.is_vip &&
               resp.hollywood.base.level == 3) ? "PASS" : "FAIL");
  if (!(resp.has_hollywood && resp.hollywood.has_base && resp.hollywood.base.is_vip &&
        resp.hollywood.base.level == 3)) ++g_failures;

  // response_error.
  check_eq(vip::response_error(from_hex(vacc::RESP_ERR_HEAD_HEX)), vacc::RESP_ERR_EXPECTED,
           "response_error (ret<0 + msg)");
  check_eq(vip::response_error(from_hex(vacc::RESP_OK_HEAD_HEX)), "",
           "response_error (không lỗi -> rỗng)");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
