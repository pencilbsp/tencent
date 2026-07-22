// test_turing_material.cpp — K26/K34 native-material byte-exact vectors.
#include <cstdio>
#include <string>

#include "tencent/core/common/json.h"
#include "tencent/core/protocol/identity/turing_build.h"
#include "tencent/core/common/hex.h"
#include "tencent/core/model/turing_material.h"

namespace tur = tencent::turing;

static int g_fail = 0;
static void check(bool ok, const std::string& name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) ++g_fail;
}
static void eq(const std::string& got, const std::string& want, const std::string& name) {
  bool ok = got == want;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) { ++g_fail; std::printf("    want=%s got=%s\n", want.c_str(), got.c_str()); }
}

int main() {
  std::printf("== turing_material ==\n");

  const std::string uuid = "849db351-83e7-4ff4-918c-94459ffbfc79";
  const std::string cipher_hex =
      "3b5fb61d7e6f54a88a6c61a503f01705d39d2877d48be12c699aaaeb1378ea01"
      "db04867b62fe38503f97faa67a2f16196bc6fd187b41dc73b538747d6c00ad81";
  eq(tencent::util::hex_encode(tur::encode_k26_cache(uuid)), cipher_hex,
     "encode_k26_cache proof vector");

  tur::K26Decoded dec = tur::decode_k26_cache(tencent::util::hex_to_string(cipher_hex));
  check(dec.status == 0, "decode_k26_cache status OK");
  eq(dec.uuid_dashed, uuid, "decode_k26_cache UUID");
  eq(tur::k26_wire_from_uuid(uuid), "849db35183e74ff4918c94459ffbfc79", "K26 wire UUID");

  std::string tail = tur::build_k34_native_tail(
      1, 2, 18, {-1, 0, -1, -1, -1}, {-1, 0, -1, -1, 0}, 0);
  eq(tail, "EDB:1_2_18,NRF:-1_-1_-1_0_-1,NSF:0_-1_-1_0_-1,UD:0",
     "K34 tail print order");

  auto cl = tur::decode_k34_cl_cache(tencent::util::hex_to_string("00391c201d"));
  check(cl.has_value(), "decode_k34_cl_cache present");
  check(cl && cl->first == 57 && cl->second == 29, "decode_k34_cl_cache 57_29");

  eq(tur::decode_gooseberry_pref("a496885986d139fd"), "0_1",
     "decode_gooseberry_pref synthetic vector");
  auto prefs = tur::parse_turing_prefs_xml(
      "<map><string name=\"204\">a496885986d139fd</string>"
      "<string name=\"503\">fb21f10610ad888b</string>"
      "<int name=\"2009\" value=\"1\" /></map>");
  eq(prefs["204"], "0_1", "parse encrypted Turing string pref");
  eq(prefs["503"], "1", "parse second encrypted Turing string pref");
  eq(prefs["2009"], "1", "parse scalar Turing pref");

  tur::Material material;
  material.package_uid = 10215;
  material.android_user_id = 0;
  material.k26_wire = "849db35183e74ff4918c94459ffbfc79";
  material.k34_cl = "57_29";
  material.k34_r = "0_1";
  material.k34_us_503 = 1;
  material.k34_us_502 = 10215;
  material.k34_native_tail = tail;
  nlohmann::json raw = {
      {"device", {{"current_time_ms", "1780992184515"}}},
      {"app", {{"qqlive_uid", "10215"}, {"android_user_id", "0"}}},
  };
  auto k = tur::build_k_values(raw, &material);
  eq(k[26], material.k26_wire, "K26 reuses persisted/root material");
  check(k[34].find("CL:57_29,R:0_1,US:1_10215_10215") != std::string::npos,
        "K34 uses root Java-prefix material");
  check(k[34].find(tail) != std::string::npos,
        "K34 uses root native-tail material");

  std::printf("\n%s (%d failure(s))\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
  return g_fail == 0 ? 0 : 1;
}
