// test_params.cpp — Test cho getvinfo_params body builder (Pha 4).
//
// So byte-exact với Python getvinfo_params (generated_vectors.h). Phần hội tụ:
// flowid, encode_body (URL-encode Java), generate_ckey, build_query_params
// (normal+history, overlay/path/extra). Xem AGENTS.md §0.
#include <cstdio>
#include <optional>
#include <string>

#include "tencent/core/protocol/playback/capability.h"          // serialize_params(OrderedKV)
#include "vectors/generated_vectors.h"
#include "tencent/core/protocol/playback/request_context.h"
#include "tencent/core/protocol/playback/request_builder.h"
#include "tencent/core/common/hex.h"

using namespace tencent;
using namespace tencent::playback;   // GetvinfoContext, BuildOptions, ckey…

static int g_failures = 0;
static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}
static void check_eq(const std::string& got, const char* want, const char* name) {
  bool ok = got == std::string(want);
  check(ok, name);
  if (!ok) {
    std::printf("    want: %s\n", want);
    std::printf("    got : %s\n", got.c_str());
  }
}
static Bytes HB(const char* hex) { return util::hex_to_bytes(std::string(hex)); }

static GetvinfoContext make_test_context() {
  GetvinfoContext context;
  context.preamble_hex = "000102030405060708090a0b0c0d0e0f1011121314151617";
  context.raw_bitmap = 7;
  context.raw_ints = {0, 0, 0};
  context.trailing_hex = "0b00";
  context.guid = "GUID123";
  context.qimei36 = "QIMEI36";
  context.device_model = "SM-G991N";
  context.query_params = {
      {"vid", "OLDVID"},         {"cid", "oldcid"},
      {"guid", "GUID123"},       {"qimei", "QIMEI36"},
      {"abuserid", "AB123"},     {"userid", "U1"},
      {"openid", "O1"},          {"viptype", "1"},
      {"user_level", "2"},       {"detail_user_level", "3"},
      {"rec_user_level", "4"},   {"adpass", "ADPASSOLD"},
      {"platform", "10303"},     {"sdtfrom", "v5000"},
      {"encryptVer", "4.2"},     {"spvideo", "1"},
      {"drm", "1"},              {"defn", "hd"},
      {"defnswitch", "9"},       {"newnettype", "1"},
      {"ipstack", "3"},          {"bitrate", "5000"},
      {"atime", "0"},            {"mediascene", "OLDSCENE"},
      {"reportparams", "OLDRP"}, {"previd", "PREVIDJSON"},
      {"previd_type", "0"},      {"costdivide", "oldcost"},
      {"scene", "oldscene"},     {"flowid", "oldflow"},
      {"cKey", "oldckey"},
  };
  context.cookies = "video_platform=2;main_login=wx;ctime=123;";
  context.headers = {
      {"Host", "vv6.video.qq.com"},
      {"User-Agent", "qqlive"},
      {"Accept-Encoding", "gzip"},
  };
  return context;
}

int main() {
  std::printf("== Pha 4: getvinfo_params body builder ==\n");

  // ── flowid ───────────────────────────────────────────────────────────────
  check_eq(make_flowid(vec::FLOWID_PLATFORM, vec::FLOWID_UUID, vec::FLOWID_NANO),
           vec::FLOWID_EXPECTED, "make_flowid");
  check_eq(make_flowid(vec::FLOWID_PLATFORM, vec::FLOWID_UUID, vec::FLOWID_NANO2),
           vec::FLOWID_EXPECTED2, "make_flowid zero-pad 14");

  // ── encode_body (URL-encode kiểu Java) ────────────────────────────────────
  OrderedKV enc = {
      {"a", "hello world"}, {"b", "x=1&y=2"}, {"c", "1080*2176"}, {"d", "a/b:c"},
      {"e", "caf\xC3\xA9"}, {"f", "~tilde"},  {"g", "100%done"},  {"h", ""},
  };
  check_eq(encode_body(enc), vec::ENCBODY_EXPECTED, "encode_body (space/=/&/*/~/utf8/%)");

  // ── generate_ckey tất định ────────────────────────────────────────────────
  GetvinfoContext ckt = make_test_context();
  CkeyEntropy ent;
  ent.param2 = HB(vec::CKEY_GEN_PARAM2_HEX);
  ent.rand21 = HB(vec::CKEY_GEN_RAND21_HEX);
  ent.field16_dd_random8 = HB(vec::CKEY_GEN_DD_RANDOM8_HEX);
  ent.tv_usec = vec::CKEY_GEN_TVUSEC;
  Bytes r2 = HB(vec::CKEY_GEN_RAND2_HEX);
  ent.pad.rand2[0] = r2[0];
  ent.pad.rand2[1] = r2[1];
  ent.pad.head_rand1 = vec::CKEY_GEN_HEAD1;
  std::string generated_ckey =
      generate_ckey(ckt, vec::CKEY_GEN_VID, /*bind_vid=*/true,
                    /*keep_template_param2=*/false, ent);
  check_eq(generated_ckey, vec::CKEY_GEN_EXPECTED,
           "generate_ckey (mutate field + encode)");
  CkeyDecoded generated_decoded =
      decode_ckey(generated_ckey, ckt.xor_key(), ckt.tea_key());
  uint32_t expected_preamble_epoch = 1780470351u;
  bool preamble_epoch_ok = generated_decoded.preamble.size() == 24 &&
                           generated_decoded.preamble[20] ==
                               uint8_t(expected_preamble_epoch >> 24) &&
                           generated_decoded.preamble[21] ==
                               uint8_t(expected_preamble_epoch >> 16) &&
                           generated_decoded.preamble[22] ==
                               uint8_t(expected_preamble_epoch >> 8) &&
                           generated_decoded.preamble[23] ==
                               uint8_t(expected_preamble_epoch);
  check(preamble_epoch_ok, "generate_ckey refreshes preamble epoch");

  GetvinfoContext missing_model = ckt;
  missing_model.device_model.clear();
  bool missing_model_threw = false;
  try {
    generate_ckey(missing_model, vec::CKEY_GEN_VID, true, false, ent);
  } catch (const std::invalid_argument&) {
    missing_model_threw = true;
  }
  check(missing_model_threw, "generate_ckey rejects missing account device model");

  GetvinfoContext custom_device = ckt;
  custom_device.device_model = "ACCOUNT-MODEL-X";
  BuildOptions custom_opt;
  custom_opt.vid = "DEVICEVID";
  custom_opt.flowid = std::string(vec::BUILD_FIXED_FLOW);
  custom_opt.ckey = std::string(vec::BUILD_FIXED_CKEY);
  OrderedKV custom_params = build_query_params(custom_device, custom_opt);
  std::string custom_model;
  for (const auto& [key, value] : custom_params)
    if (key == "devicemodel") custom_model = value;
  check(custom_model == "ACCOUNT-MODEL-X",
        "build_query_params preserves account device model");

  // ── build_query_params NORMAL ─────────────────────────────────────────────
  BuildOptions on;
  on.vid = "NEWVID";
  on.cid = std::string("NEWCID");
  on.defn = "uhd";
  on.scene = "playerUiType=0";
  on.mode = "normal";
  on.atime = std::string("111");
  on.bitrate = std::string("222");
  on.flowid = std::string(vec::BUILD_FIXED_FLOW);
  on.ckey = std::string(vec::BUILD_FIXED_CKEY);
  on.extra = OptionalKV{{"spvideo", std::string("7777")},
                        {"drm", std::nullopt},
                        {"extraNew", std::string("E1")}};
  OrderedKV pn = build_query_params(ckt, on);
  check_eq(serialize_params(pn), vec::BUILD_NORMAL, "build_query_params NORMAL");
  check_eq(encode_body(pn), vec::BUILD_NORMAL_BODY, "build_full_request body NORMAL");

  // ── build_query_params HISTORY ────────────────────────────────────────────
  BuildOptions oh;
  oh.vid = "NEWVID";
  oh.defn = "hd";
  oh.mode = "history";
  oh.flowid = std::string(vec::BUILD_FIXED_FLOW);
  oh.ckey = std::string(vec::BUILD_FIXED_CKEY);
  oh.previd_json = std::string("{\"h\":\"1\"}");
  OrderedKV ph = build_query_params(ckt, oh);
  check_eq(serialize_params(ph), vec::BUILD_HISTORY, "build_query_params HISTORY");

  // ── Negative: mode lạ / history thiếu previd ──────────────────────────────
  bool threw = false;
  BuildOptions ob;
  ob.vid = "X";
  ob.mode = "bogus";
  try { build_query_params(ckt, ob); } catch (const std::exception&) { threw = true; }
  check(threw, "mode lạ ném lỗi");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
