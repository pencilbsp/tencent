// test_trpc_head.cpp — Test byte-exact cho trpc_head (Pha B).
//
// So build_frame / build_qqlive_head / build_qqlive_frame với ground-truth Python
// (../analytics/app/login_refresh_proto.py + login_refresh_head.py) trên session TỔNG HỢP
// (không secret) trong generated_vectors_acct.h. Xem AGENTS.md §0.
#include <cstdio>
#include <stdexcept>
#include <string>

#include "vectors/generated_vectors_acct.h"
#include "tencent/core/protocol/trpc/head.h"

using Session = tencent::model::SessionMaterial;

static int g_failures = 0;
static void check_eq_hex(const std::string& got_bytes, const char* want_hex, const char* name);

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
static void check_eq_hex(const std::string& got_bytes, const char* want_hex, const char* name) {
  std::string got = to_hex(got_bytes);
  bool ok = got == std::string(want_hex);
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) {
    ++g_failures;
    std::printf("    want: %s\n", want_hex);
    std::printf("    got : %s\n", got.c_str());
  }
}

int main() {
  std::printf("== Pha B: trpc_head ==\n");

  // Dựng Session từ KEYS/VALS (đảm bảo input trùng Python tuyệt đối).
  Session s;
  for (std::size_t i = 0; i < vacc::TRPC_SESSION_KEYS.size(); ++i)
    s.kv[vacc::TRPC_SESSION_KEYS[i]] = vacc::TRPC_SESSION_VALS[i];
  s.kv["model"] = "SM-G991N";
  s.kv["brand"] = "samsung";
  s.kv["os_version"] = "14";
  s.kv["android_api_level"] = "34";
  s.kv["app_ver"] = "9.02.90.31109";
  s.kv["app_build"] = "31109";
  s.kv["app_sub_build"] = "20563";
  s.kv["platform"] = "1000005";
  s.kv["screen_width"] = "1080";
  s.kv["screen_height"] = "2176";
  s.kv["screen_dpi"] = "480";

  tencent::OrderedKV extra = {{vacc::TRPC_EXTRA_KEY, vacc::TRPC_EXTRA_VAL}};

  // build_frame trực tiếp.
  check_eq_hex(tencent::trpc::build_frame(from_hex(vacc::BUILD_FRAME_IN_HEX),
                                          vacc::BUILD_FRAME_HEADLEN),
               vacc::BUILD_FRAME_OUT_HEX, "build_frame(bytes(20), headLen=12)");

  // build_qqlive_head (ts_ms nằm trong session -> now_ms không dùng, truyền 0).
  std::string head = tencent::trpc::build_qqlive_head(
      vacc::TRPC_SERVICE, vacc::TRPC_METHOD, s, vacc::TRPC_PROTOCOL_ID, 0, extra);
  check_eq_hex(head, vacc::TRPC_HEAD_HEX, "build_qqlive_head (synthetic session)");

  // build_qqlive_frame.
  std::string frame = tencent::trpc::build_qqlive_frame(
      vacc::TRPC_SERVICE, vacc::TRPC_METHOD, from_hex(vacc::TRPC_BODY_HEX), s,
      vacc::TRPC_PROTOCOL_ID, 0, extra);
  check_eq_hex(frame, vacc::TRPC_FRAME_HEX, "build_qqlive_frame (head+body+16B frame)");

  Session missing_device = s;
  missing_device.kv.erase("screen_dpi");
  bool threw = false;
  try {
    tencent::trpc::build_qqlive_head(
        vacc::TRPC_SERVICE, vacc::TRPC_METHOD, missing_device,
        vacc::TRPC_PROTOCOL_ID, 0, extra);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  std::printf("  [%s] missing device field is rejected\n", threw ? "PASS" : "FAIL");
  if (!threw) ++g_failures;

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
