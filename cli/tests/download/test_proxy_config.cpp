#include <cstdio>
#include <string>

#include "tencent/core/download/proxy_config.h"

namespace dp = tencent::download_proxy;

static int failures = 0;

static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++failures;
}

int main() {
  dp::Context context{"3.14.0.02629", "10303", "GUID", "QIMEI36",
                      "sdk_gphone64_arm64"};
  check(dp::build_config_url(context) ==
            "https://appcfg.v.qq.com/getconf?cmd=data_transport&name=tp_proxy"
            "&subver=V_3.14.0.02629&platform=10303&guid=GUID"
            "&qimei36=QIMEI36&dev=sdk%5Fgphone64%5Farm64",
        "build exact appcfg URL");

  dp::Config explicit_config = dp::parse_config_response(
      R"({"code":0,"data":"{\"CDNStrategy\":2,\"QuickDownloadCDNStrategy\":1}"})");
  check(explicit_config.has_cdn_strategy && explicit_config.cdn_strategy == 2,
        "parse CDNStrategy");
  check(explicit_config.has_quick_download_cdn_strategy &&
            explicit_config.quick_download_cdn_strategy == 1,
        "parse QuickDownloadCDNStrategy");

  dp::Config defaults =
      dp::parse_config_response(R"({"code":0,"data":"{\"EnableHighCodeRate\":true}"})");
  check(!defaults.has_cdn_strategy && defaults.cdn_strategy == 0,
        "missing CDNStrategy keeps native default");

  std::printf("\n%s (%d failure(s))\n",
              failures == 0 ? "ALL PASS" : "FAILED", failures);
  return failures == 0 ? 0 : 1;
}
