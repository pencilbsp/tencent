// turing_material.h — Vật liệu Turing native lấy về bằng `auth --adb`.
//
// Đây là trạng thái THIẾT BỊ/ỨNG DỤNG (không phải credential tài khoản), được
// lưu kèm account để đường Mint dựng được request tươi với đúng trạng thái
// cache K26/K34 như trong app — thay vì hardcode.
//
// Vị trí trong kiến trúc: tầng `model/` = dữ liệu THUẦN + thao tác giải mã cục
// bộ trên chính dữ liệu đó. Việc LẤY vật liệu về từ thiết bị nằm ở
// `device/helper_probe.h`; việc DỰNG K-value cho request nằm ở
// `protocol/identity/turing_build.h`.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tencent::turing {

inline constexpr const char* kK26XxteaKeyHex = "f15e78d23033c2c4bac8a925677e863a";

struct K26Decoded {
  int status = 0;              // 0 OK, otherwise native-style negative error.
  std::string uuid_dashed;     // key "1", dashed lowercase UUID.
  std::string aux;             // key "2" if present.
};

struct K26Node {
  int index = 0;               // Native node index.
  std::string path;
  int64_t size = 0;
  int read_status = -11002;    // -11002 no file/read fail; 0 means bytes read.
  int decode_status = -31000;  // 0 only when UUID was decoded.
  std::string uuid_dashed;
  std::string blob_hex;        // Existing cache bytes, for offline replay/debug.
};

struct Material {
  int64_t package_uid = 0;
  int android_user_id = 0;

  std::string k26_uuid_dashed;
  std::string k26_wire;        // UUID with hyphens removed.
  std::string k26_source;      // "root-cache", "persisted", or "synthesized".
  std::string k26_cache_blob_hex;
  std::vector<K26Node> k26_nodes;

  int k34_edb_state = 0;
  int k34_load_bitmap = 0;
  int k34_save_bitmap = 0;
  std::vector<int> k34_nrf{-1, -1, -1, -1, -1};  // Internal order.
  std::vector<int> k34_nsf{-1, -1, -1, -1, -1};  // Internal order.
  int k34_ud = 0;
  std::string k34_native_tail;
  int k34_cl_a = 0;
  int k34_cl_c = 0;
  std::string k34_cl;
  std::string k34_cl_cache_hex;
  std::string k34_r;
  int k34_us_503 = 0;
  int k34_us_502 = 0;
  std::string k34_stif;
};

// LƯU Ý: hex_encode/hex_decode trước đây khai báo ở đây đã chuyển sang
// common/hex.h (util::hex_encode / util::hex_to_string) — chúng là bản sao y
// hệt của bản trong tencent_crypto.cpp.

std::string normalize_k26_uuid(const std::string& value);
std::string k26_wire_from_uuid(const std::string& dashed_uuid);

std::string build_k26_cache_plain(const std::string& dashed_uuid,
                                  const std::string& aux = "");
std::string encode_k26_cache(const std::string& dashed_uuid,
                             const std::string& aux = "");
K26Decoded decode_k26_cache(const std::string& blob);

std::optional<std::pair<int, int>> decode_k34_cl_cache(const std::string& data);
std::string decode_gooseberry_pref(const std::string& hex_value);
std::map<std::string, std::string> parse_turing_prefs_xml(const std::string& xml);

// Cờ bật/tắt trong prefs Turing: chấp nhận 1/true/yes/y/on (không phân biệt hoa
// thường). Khoá rỗng = mặc định BẬT (native không kiểm tra gì).
bool pref_enabled(const std::map<std::string, std::string>& prefs, const std::string& key);
std::string build_k34_native_tail(int edb_state, int load_bitmap, int save_bitmap,
                                  const std::vector<int>& nrf,
                                  const std::vector<int>& nsf,
                                  int ud);

std::vector<std::string> k26_cache_paths(const std::string& pkg);

}  // namespace tencent::turing
