#include <cstdio>
#include <string>

#include "tencent/core/protocol/identity/qimei_client.h"

static int failures = 0;
static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++failures;
}

int main() {
  std::printf("== qimei_client ==\n");
  tencent::qimei::DeviceProfile p;
  p.app_version = "9.02.90.31109";
  p.android_version = "14";
  p.android_api_level = "34";
  p.model = "SM-G991N";
  p.manufacturer = "samsung";
  p.brand = "samsung";
  p.android_id = "fcb1ad2ab041a2ef";
  p.kernel = "Linux localhost 5.4.302 test aarch64";
  p.boot_id = "ac9c15f6-9fdc-4322-8c03-b2a865c7eef8";
  p.device_signal_kv =
      "k1:2026-03-23171055.871999996+0700;"
      "k2:2026-03-23171234.283999926+0700;"
      "k3:0000000000000000;"
      "k9:ac9c15f6-9fdc-4322-8c03-b2a865c7eef8;"
      "k10:1";
  p.current_time_ms = 1781250032657;
  p.elapsed_realtime_ms = 423898993;

  const std::string tail17 = tencent::qimei::build_tail17(p);
  check(tail17 ==
            "d344ec31eca87bd6ec76f98e76b6a3ebeb508f317b868344b03d4d31a2a2d3d3",
        "field17 deterministic vector");
  std::string plain = tencent::qimei::build_register_plaintext(
      p, "00112233445566778899aabbccdd1", 1781250000);
  check(plain.size() == 1090, "REGISTER plaintext length");
  check(tencent::qimei::make_sn("test-preimage", 0x2251b) ==
            "4c43851046f90002251b15a6abd1753a9609f523c6e90bdb1a540000000000000000",
        "sn deterministic vector");

  const std::string raw = R"({
    "props":{
      "ro.build.version.release":"14","ro.build.version.sdk":"34",
      "ro.product.model":"SM-G991N","ro.product.manufacturer":"samsung",
      "ro.product.brand":"samsung","ro.board.first_api_level":"30"
    },
    "device":{
      "current_time_ms":"1781250032657","elapsed_realtime_ms":"423898993",
      "kernel":"Linux localhost test","timezone_offset_minutes":"420"
    },
    "app":{"qqlive_version_name":"9.02.90.31109"},
    "files":{"boot_id":"827ce166-6e78-49d9-b932-af8d062707b0"},
    "packages":[
      {"name":"com.tencent.qqlive","android_id":"fcb1ad2ab041a2ef",
       "first_install_ms":"1778206151549"}
    ],
    "qimei":{
      "stat_records":[
        {"index":"0","raw":"1774260655|1774260754|0|137|4|2026-03-23 17:10:55.871999996 +0700|2026-03-23 17:12:34.283999926 +0700|"},
        {"index":"1","raw":"0|59086331|12812797"}
      ]
    }
  })";
  tencent::qimei::DeviceProfile parsed =
      tencent::qimei::profile_from_raw_json(raw);
  check(parsed.first_api_level == "30", "first API property fallback");
  check(parsed.android_id == "fcb1ad2ab041a2ef", "android id from package record");
  check(parsed.kernel == "Linux localhost test", "kernel from device group");
  check(parsed.timezone_offset_minutes == 420, "timezone from device group");
  check(parsed.device_signal_kv ==
            "k1:2026-03-23171055.871999996+0700;"
            "k2:2026-03-23171234.283999926+0700;"
            "k3:0000000000000000;k5:59086331;k6:12812797;"
            "k9:827ce166-6e78-49d9-b932-af8d062707b0;k11:137;k12:4;k10:1",
        "raw stat records to native device signal");
  bool rejected_old_helper = false;
  try {
    (void)tencent::qimei::profile_from_raw_json(R"({"props":{}})");
  } catch (...) {
    rejected_old_helper = true;
  }
  check(rejected_old_helper, "old helper snapshot rejected clearly");

  bool rejected_without_root_id = false;
  try {
    std::string no_root = raw;
    std::size_t id = no_root.find("fcb1ad2ab041a2ef");
    no_root.replace(id, 16, "");
    (void)tencent::qimei::profile_from_raw_json(no_root);
  } catch (...) {
    rejected_without_root_id = true;
  }
  check(rejected_without_root_id, "missing root package android id rejected");

  std::printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "ALL PASS", failures);
  return failures ? 1 : 0;
}
