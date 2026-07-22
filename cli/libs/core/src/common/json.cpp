// json.cpp — Xem include/tencent/core/common/json.h.
#include "tencent/core/common/json.h"

#include "tencent/core/common/strings.h"

namespace tencent::util {
namespace {

// Thân chung cho cả json lẫn ordered_json (hai kiểu khác nhau nhưng cùng API).
template <typename JsonType>
int64_t read_int(const JsonType& object, const char* key, int64_t fallback) {
  if (!object.is_object() || !object.contains(key)) return fallback;
  const auto& node = object[key];
  if (node.is_number_integer() || node.is_number_unsigned()) return node.template get<int64_t>();
  if (node.is_number_float()) return static_cast<int64_t>(node.template get<double>());
  if (node.is_string()) return to_int64(node.template get<std::string>(), fallback);
  if (node.is_boolean()) return node.template get<bool>() ? 1 : 0;
  return fallback;
}

template <typename JsonType>
std::string read_string(const JsonType& object, const char* key,
                        const std::string& fallback) {
  if (!object.is_object() || !object.contains(key)) return fallback;
  const auto& node = object[key];
  if (node.is_string()) return node.template get<std::string>();
  if (node.is_null()) return fallback;
  if (node.is_number_integer() || node.is_number_unsigned()) {
    return std::to_string(node.template get<int64_t>());
  }
  if (node.is_boolean()) return node.template get<bool>() ? "true" : "false";
  return fallback;
}

}  // namespace

int64_t json_int(const Json& object, const char* key, int64_t fallback) {
  return read_int(object, key, fallback);
}

int64_t json_int(const OrderedJson& object, const char* key, int64_t fallback) {
  return read_int(object, key, fallback);
}

std::string json_string(const Json& object, const char* key, const std::string& fallback) {
  return read_string(object, key, fallback);
}

std::string json_string(const OrderedJson& object, const char* key,
                        const std::string& fallback) {
  return read_string(object, key, fallback);
}

bool json_bool(const Json& object, const char* key, bool fallback) {
  if (!object.is_object() || !object.contains(key)) return fallback;
  const auto& node = object[key];
  if (node.is_boolean()) return node.get<bool>();
  if (node.is_number()) return node.get<int64_t>() != 0;
  if (node.is_string()) {
    const std::string text = to_lower(node.get<std::string>());
    return text == "true" || text == "1" || text == "yes";
  }
  return fallback;
}

}  // namespace tencent::util
