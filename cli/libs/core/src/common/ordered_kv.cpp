// ordered_kv.cpp — Xem include/tencent/core/common/ordered_kv.h.
#include "tencent/core/common/ordered_kv.h"

namespace tencent {

const std::string* find_value(const OrderedKV& items, const std::string& key) {
  for (const auto& [item_key, item_value] : items) {
    if (item_key == key) return &item_value;
  }
  return nullptr;
}

std::string value_or(const OrderedKV& items, const std::string& key,
                     const std::string& fallback) {
  const std::string* found = find_value(items, key);
  if (!found || found->empty()) return fallback;
  return *found;
}

void set_value(OrderedKV& items, const std::string& key, const std::string& value) {
  for (auto& item : items) {
    if (item.first == key) {
      item.second = value;
      return;
    }
  }
  items.emplace_back(key, value);
}

void set_value(OptionalKV& items, const std::string& key,
               const std::optional<std::string>& value) {
  for (auto& item : items) {
    if (item.first == key) {
      item.second = value;
      return;
    }
  }
  items.emplace_back(key, value);
}

bool erase_key(OrderedKV& items, const std::string& key) {
  for (auto it = items.begin(); it != items.end(); ++it) {
    if (it->first == key) {
      items.erase(it);
      return true;
    }
  }
  return false;
}

}  // namespace tencent
