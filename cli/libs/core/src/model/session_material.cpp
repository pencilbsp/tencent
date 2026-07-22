// session_material.cpp — Xem include/tencent/core/model/session_material.h.
// Thân hàm chuyển nguyên từ trpc_head.cpp cũ (khối `Session`).
#include "tencent/core/model/session_material.h"

#include "tencent/core/common/strings.h"

namespace tencent::model {

bool SessionMaterial::has(const std::string& key) const {
  return kv.find(key) != kv.end();
}

std::string SessionMaterial::get(const std::string& key, const std::string& fallback) const {
  const auto it = kv.find(key);
  return it == kv.end() ? fallback : it->second;
}

// ../analytics/app/login_refresh_head.py:69-74 (_pick: giá trị đầu khác None/"").
std::string SessionMaterial::pick(std::initializer_list<const char*> keys,
                                  const std::string& fallback) const {
  for (const char* key : keys) {
    const auto it = kv.find(key);
    if (it != kv.end() && !it->second.empty()) return it->second;
  }
  return fallback;
}

long SessionMaterial::get_int(const std::string& key, long fallback) const {
  const auto it = kv.find(key);
  if (it == kv.end() || it->second.empty()) return fallback;
  // Bản cũ dùng std::stol và sẽ NÉM nếu giá trị không phải số; ở đây trả về
  // fallback cho an toàn — dữ liệu dump từ thiết bị không phải lúc nào cũng sạch.
  return static_cast<long>(util::to_int64(it->second, fallback));
}

}  // namespace tencent::model
