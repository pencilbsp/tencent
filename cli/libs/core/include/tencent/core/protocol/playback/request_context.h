#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tencent/core/protocol/playback/constants.h"
#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/common/bytes.h"
#include "tencent/core/common/hex.h"
#include "tencent/core/model/account.h"

namespace tencent::playback {

// Runtime input assembled from static protocol constants and one account bundle.
struct GetvinfoContext {
  std::string preamble_hex;
  uint32_t raw_bitmap = 0x00000007;
  std::vector<uint32_t> raw_ints;
  std::string trailing_hex;

  std::string guid;
  std::string qimei36;
  std::string device_model;

  OrderedKV query_params;
  std::string cookies;
  OrderedKV headers;

  Bytes xor_key() const { return util::hex_to_bytes(constants::kCkeyXorKeyHex); }
  Bytes tea_key() const { return util::hex_to_bytes(constants::kCkeyTeaKeyHex); }
  Bytes preamble() const { return util::hex_to_bytes(preamble_hex); }
  Bytes trailing() const { return util::hex_to_bytes(trailing_hex); }

  const std::string* find_param(const std::string& key) const {
    for (const auto& [param_key, value] : query_params) {
      if (param_key == key) return &value;
    }
    return nullptr;
  }
};

// Dựng ngữ cảnh request từ HẰNG SỐ giao thức + một account cụ thể.
//
// Đây là chỗ duy nhất ghép hai nguồn dữ liệu lại: constants mang tính per-build
// (khoá TEA/XOR, preamble, trailing, danh sách param mặc định) còn account mang
// tính per-device/per-user (guid, qimei, model, cookie đăng nhập).
// `now_ms` được truyền vào để cookie sinh ra tất định khi test.
GetvinfoContext build_request_context(const account::Account& account, int64_t now_ms);

}  // namespace tencent::playback
