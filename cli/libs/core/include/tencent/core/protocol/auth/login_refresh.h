// login_refresh.h — Làm mới vusession (login-refresh) đầu-cuối.
//
// Port ../analytics/app/login_refresh_proto.py (build_refresh_request, proto3 RefreshRequest)
// + ../analytics/app/login_refresh_response.py (parse RefreshResponse) + bộ điều phối
// ../analytics/app/login_refresh.py (build_refresh_frame, đường build_qqlive_frame).
//
// Kết hợp: body (proto3) + qqlive_head (trpc_head) + sign (login_sign energy) →
// khung VBPB 16B sẵn sàng gzip+POST a.video.qq.com. Đáp ứng trả về session/
// refresh/access_token MỚI. Xem ../analytics/app/login_refresh_flow.md.
//
// Random/clock (now_ms, rnd, usec) INJECT để test byte-exact (AGENTS.md §0).
#pragma once

#include <cstdint>
#include <string>

#include "tencent/core/protocol/trpc/head.h"

namespace tencent::auth {

// Endpoint refresh (../analytics/app/login_refresh.py:39-41).
inline constexpr const char* kRefreshService = "trpc.anywhere_door.account.Account";
inline constexpr const char* kRefreshMethod = "/trpc.anywhere_door.account.Account/Refresh";
inline constexpr int kRefreshProtocolId = 14;
inline constexpr int kThirdTypeWX = 2;  // ../analytics/app/login_refresh_proto.py:38

// ── RefreshRequest body (proto3) ─────────────────────────────────────────────
// Port build_refresh_request (../analytics/app/login_refresh_proto.py:142-176). Mọi field
// OMIT_IDENTITY: chỉ phát khi khác mặc định (int64!=0, string!="", msg!=null).
// sign_info chỉ phát khi có qimei36/sign/original_data/h38.
std::string build_refresh_request(int64_t vuid, const std::string& vusession,
                                  const std::string& vurefresh,
                                  const std::string& wx_app_id,
                                  const std::string& wx_open_id,
                                  const std::string& wx_access_token,
                                  const std::string& wx_refresh_token,
                                  const std::string& qimei36,
                                  const std::string& original_data,
                                  const std::string& sign, int third_type = kThirdTypeWX,
                                  const std::string& h38 = "",
                                  bool include_third_info = true);

// Dựng refresh frame từ Session ĐÃ CÓ đầy đủ device/app fingerprint:
// đọc vuid/vusession/vurefresh/wx_*/qimei36/guid/usid + model/app_ver/platform…
// từ `base`. Dùng cho tencent-cli auth --refresh (account_file::to_session()).
std::string build_refresh_frame_from_session(const model::SessionMaterial& base, uint64_t now_ms,
                                             uint32_t rnd, uint32_t usec,
                                             const std::string& sign = "");

// ── Parse RefreshResponse (../analytics/app/login_refresh_response.py) ──────────────────
struct RefreshResult {
  int64_t vuid = 0;
  std::string vusession;                       // session key MỚI
  int64_t vusession_expire_timestamp = 0;      // giây (×1000 - now = hạn ms)
  std::string vurefresh;                       // refresh key MỚI
  std::string third_app_id;
  std::string third_open_id;
  std::string third_access_token;              // access_token MỚI
  std::string third_refresh_token;
  bool has_trpc_ret = false;
  int64_t trpc_ret = 0;                        // mã trả TRPC head (0 = OK)
};

// Bỏ khung 16B VBPB; trả (trpc_head, refresh_body) (login_refresh_response.py:42-53).
// Ném std::runtime_error nếu magic/độ dài sai.
void unframe_response(const std::string& resp, std::string& head, std::string& body);

// parse_refresh_response (login_refresh_response.py:104-121).
RefreshResult parse_refresh_response(const std::string& resp);

}  // namespace tencent::auth
