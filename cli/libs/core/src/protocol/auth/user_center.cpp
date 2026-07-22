// usercenter.cpp - Minimal schema-aware parser for the personal-center diamond block.
#include "tencent/core/protocol/auth/user_center.h"

#include <vector>

#include "tencent/core/codec/protobuf.h"

namespace tencent::usercenter {

namespace pb = tencent::pb;

static std::string map_entry(const std::string& key, const std::string& value) {
  return pb::serialize({pb::Field::bytes(1, key), pb::Field::bytes(2, value)});
}

std::string build_page_request(const std::string& vuid) {
  std::vector<pb::Field> fields;
  fields.push_back(pb::Field::bytes(1, map_entry("page_id", "default")));
  fields.push_back(pb::Field::bytes(1, map_entry("page_type", "personal")));
  fields.push_back(pb::Field::bytes(1, map_entry("main_vuid", vuid)));
  fields.push_back(pb::Field::bytes(1, map_entry("login_vuid", vuid)));
  return pb::serialize(fields);
}

static std::string diamond_from_user_info(const std::string& user_info_data) {
  // UserCenterUserInfo.vip_info(3) -> UserCenterVIPInfo.diamond_block(3)
  // -> UserCenterVIPBlock.sub_title(2).
  const std::vector<pb::Field> user_info = pb::parse(user_info_data);
  const pb::Field* vip_info_data = pb::get_first(user_info, 3);
  if (!vip_info_data || vip_info_data->wire != 2) return {};
  const std::vector<pb::Field> vip_info = pb::parse(vip_info_data->data);
  const pb::Field* diamond_data = pb::get_first(vip_info, 3);
  if (!diamond_data || diamond_data->wire != 2) return {};
  const std::vector<pb::Field> diamond = pb::parse(diamond_data->data);
  const pb::Field* balance = pb::get_first(diamond, 2);
  return (balance && balance->wire == 2) ? balance->data : std::string{};
}

static std::string diamond_from_card(const std::string& card_data) {
  const std::vector<pb::Field> card = pb::parse(card_data);
  const pb::Field* type = pb::get_first(card, 2);
  if (!type || type->wire != 2 || type->data != "user_center_user_info") return {};

  // Card.data (tag 15) is google.protobuf.Any. Its value contains
  // UserCenter.home_page(1).user_info(2), not UserCenterUserInfo directly.
  const pb::Field* any_field = pb::get_first(card, 15);
  if (!any_field || any_field->wire != 2) return {};
  const std::vector<pb::Field> any = pb::parse(any_field->data);
  const pb::Field* user_center_data = pb::get_first(any, 2);
  if (!user_center_data || user_center_data->wire != 2) return {};

  const std::vector<pb::Field> user_center = pb::parse(user_center_data->data);
  const pb::Field* home_page_data = pb::get_first(user_center, 1);
  if (home_page_data && home_page_data->wire == 2) {
    const std::vector<pb::Field> home_page = pb::parse(home_page_data->data);
    const pb::Field* user_info_data = pb::get_first(home_page, 2);
    if (user_info_data && user_info_data->wire == 2) {
      std::string balance = diamond_from_user_info(user_info_data->data);
      if (!balance.empty()) return balance;
    }
  }

  // Retain compatibility with older captures that stored UserCenterUserInfo
  // directly in Any.value.
  return diamond_from_user_info(user_center_data->data);
}

std::string extract_diamond_balance(const std::string& response_body) {
  // MVLPageResponse.modules(1) is map<string, CardList>. CardList.cards is tag 1.
  const std::vector<pb::Field> response = pb::parse(response_body);
  for (const pb::Field* module_entry_data : pb::get_all(response, 1)) {
    if (module_entry_data->wire != 2) continue;
    const std::vector<pb::Field> module_entry = pb::parse(module_entry_data->data);
    const pb::Field* card_list_data = pb::get_first(module_entry, 2);
    if (!card_list_data || card_list_data->wire != 2) continue;
    const std::vector<pb::Field> card_list = pb::parse(card_list_data->data);
    for (const pb::Field* card : pb::get_all(card_list, 1)) {
      if (card->wire != 2) continue;
      std::string balance = diamond_from_card(card->data);
      if (!balance.empty()) return balance;
    }
  }
  return {};
}

}  // namespace tencent::usercenter
