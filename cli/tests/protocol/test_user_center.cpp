#include <cstdio>
#include <string>

#include "tencent/core/codec/protobuf.h"
#include "tencent/core/protocol/auth/user_center.h"

namespace pb = tencent::pb;
namespace uc = tencent::usercenter;

static int failures = 0;

static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++failures;
}

static std::string bytes(int tag, const std::string& data) {
  return pb::Field::bytes(tag, data).encode();
}

int main() {
  std::printf("== usercenter ==\n");

  const std::string request = uc::build_page_request("3578420805");
  const std::vector<pb::Field> request_fields = pb::parse(request);
  check(pb::get_all(request_fields, 1).size() == 4,
        "request has four page_params entries");

  const std::string diamond =
      bytes(1, "  \xE9\x92\xBB\xE7\x9F\xB3\xEF\xBC\x9A") + bytes(2, "28.5");
  const std::string vip_info = bytes(3, diamond);
  const std::string user_info = bytes(3, vip_info);
  const std::string home_page = bytes(2, user_info);
  const std::string user_center = bytes(1, home_page);
  const std::string any =
      bytes(1, "type.googleapis.com/trpc.vector_layout.page_view.UserCenter") +
      bytes(2, user_center);
  const std::string card = bytes(2, "user_center_user_info") + bytes(15, any);
  const std::string card_list = bytes(1, card);
  const std::string module_entry = bytes(1, "normal") + bytes(2, card_list);
  const std::string response = bytes(1, module_entry);

  check(uc::extract_diamond_balance(response) == "28.5",
        "extract wrapped diamond_block.sub_title");

  const std::string legacy_any =
      bytes(1, "type.googleapis.com/UserCenterUserInfo") + bytes(2, user_info);
  const std::string legacy_card =
      bytes(2, "user_center_user_info") + bytes(15, legacy_any);
  const std::string legacy_response =
      bytes(1, bytes(1, "normal") + bytes(2, bytes(1, legacy_card)));
  check(uc::extract_diamond_balance(legacy_response) == "28.5",
        "extract legacy direct UserCenterUserInfo");
  check(uc::extract_diamond_balance(bytes(1, bytes(1, "normal"))) == "",
        "missing diamond block returns empty");

  std::printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILED",
              failures);
  return failures == 0 ? 0 : 1;
}
