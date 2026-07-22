#include <cstdio>
#include <string>

#include "tencent/core/protocol/identity/guid_client.h"
#include "tencent/core/common/hex.h"

int main() {
  const std::string fixture =
      "13000000c60001ff01e1800000000000006f482414000202130000271c0000000000000000"
      "62666166366666373565366531316631393136393763386330393236386663610100007985"
      "0000000000000000000000000000611f8b08000000000000035363606048646480032ea6"
      "7c0f1511210686870d3a660c1e3c113c590ccc620cba0c3cdcdcb20c0c5a3c620c6a0a49"
      "698969666969e6a6a966a98686698696866696e6c916c9069646661669c9894659da2704b"
      "47864819a3400f47fdb366100000003";
  tencent::Bytes raw = tencent::util::hex_to_bytes(fixture);
  auto result = tencent::guid::parse_response(
      std::string(reinterpret_cast<const char*>(raw.data()), raw.size()));
  bool ok = result.err_code == 0 &&
            result.guid == "bfaf6ff75e6e11f191697c8c09268fca" &&
            result.server_time == 1781254160;
  std::printf("== guid_client ==\n  [%s] parse live response fixture\n",
              ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
