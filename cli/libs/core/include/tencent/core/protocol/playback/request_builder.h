// getvinfo_params.h — Bộ ráp body request getvinfo (phần hội tụ của Pha 1-3).
//
// Port 1:1 từ ../analytics/app/getvinfo_params.py. Tạo POST body cho
// https://vv6.video.qq.com/getvinfo từ GetvinfoContext + input mỗi request.
//
// Native tham chiếu: libtvkplayer.so tvk_vinfo_build_request @ 0x4D0E08.
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "tencent/core/protocol/playback/ckey_codec.h"        // PadEntropy
#include "tencent/core/protocol/playback/request_context.h"
#include "tencent/core/protocol/playback/constants.h"
#include "tencent/core/common/ordered_kv.h"

namespace tencent::playback {

// ── flowid (getvinfo_params.py:81-91) ────────────────────────────────────────
// make_flowid: f"{uuid}{nano:014d}_{platform}". nano = 14 chữ số (zero-pad).
std::string make_flowid(const std::string& platform, const std::string& uuid_str,
                        uint64_t nano);
// Bản runtime: uuid4 ngẫu nhiên + monotonic ns % 1e14. KHÔNG test byte-exact.
std::string make_flowid_random(const std::string& platform = "10303");

// ── cKey generation (getvinfo_params.py:99-170) ──────────────────────────────
// Entropy được INJECT để test tất định (xem AGENTS.md §0/§5). Tương ứng
// fresh_param2_and_random() (py:99-106) + pad của encode_ckey.
struct CkeyEntropy {
  Bytes param2;      // Field[20] mới (trước khi căn độ dài) — str(ts_ms)+hex(8)
  Bytes rand21;      // Field[21] mới (trước khi căn độ dài) — hex(8)
  Bytes field16_dd_random8;  // random8 riêng cho Field[16].dd
  uint32_t tv_usec = 0;  // Field[19] byte 6-9
  PadEntropy pad;    // pad của encode_ckey
};

// generate_ckey tất định (port py:109-170). Đột biến field per-request rồi
// encode_ckey. bind_vid: ghi vid vào Field[4]. keep_template_param2: giữ nguyên
// field per-request của context (replay), bỏ qua param2/rand21/tv_usec.
std::string generate_ckey(const GetvinfoContext& context, const std::string& vid, bool bind_vid,
                          bool keep_template_param2, const CkeyEntropy& ent,
                          const std::string& component2 = "",
                          const std::string& qimei36 = "");
// Bản runtime: sinh entropy ngẫu nhiên. KHÔNG test byte-exact.
std::string generate_ckey_random(const GetvinfoContext& context, const std::string& vid,
                                 bool bind_vid = true, const std::string& component2 = "",
                                 const std::string& qimei36 = "");

// ── Body encoding (getvinfo_params.py:279-291) ───────────────────────────────
// URL-encode kiểu Java: giữ alnum + "_.-~*" (safe="-_.*" + always-safe của
// urllib.parse.quote), %XX hoa cho phần còn lại. Key KHÔNG encode. Join "&".
std::string url_quote(const std::string& s);
std::string encode_body(const OrderedKV& params);

// ── Lắp query params (getvinfo_params.py:185-273) ────────────────────────────
struct BuildOptions {
  std::string vid;                          // bắt buộc
  std::optional<std::string> cid;
  std::string defn = "uhd";                 // quality NGƯỜI DÙNG CHỌN
  std::string scene = "playerUiType=0";
  std::string mode = "normal";              // "normal" | "history"
  std::optional<std::string> atime;
  std::optional<std::string> bitrate;
  std::optional<std::string> flowid;        // có -> dùng; không -> sinh
  std::optional<std::string> ckey;          // có -> dùng; không -> sinh
  bool ckey_bind_vid = true;
  std::optional<std::string> ckey_component2;  // trống -> vid hiện hành
  std::optional<std::string> adpass;        // có -> dùng; không -> tự dựng QAD
  bool include_adpass = true;
  std::optional<std::string> adpass_cookie; // trống -> Cookie header của context
  std::optional<std::string> taid_ticket;   // trống -> privacy-disabled empty value
  std::optional<std::string> adpass_os_version;  // trống -> Android_<osver>
  std::optional<int> adpass_wx_version_code;
  std::optional<std::string> previd_json;   // bắt buộc nếu mode=history & context không có
  OptionalKV extra;                         // override capability (nullopt = xóa key)
};

// Ném std::invalid_argument nếu mode lạ, hoặc history thiếu previd.
OrderedKV build_query_params(const GetvinfoContext& context, const BuildOptions& opt);

// build_headers (getvinfo_params.py:294-306): header cố định + Cookie từ context.
OrderedKV build_headers(const GetvinfoContext& context);

// build_full_request (getvinfo_params.py:309-323).
struct FullRequest {
  std::string method = "POST";
  std::string url = constants::kGetvinfoUrl;
  OrderedKV headers;
  std::string body;
  OrderedKV params;
};
FullRequest build_full_request(const GetvinfoContext& context, const BuildOptions& opt);

}  // namespace tencent::playback
