// trpc_head.h — Dựng khung VBPB 16B + qqlive_head (TRPC RequestHead).
//
// Port ../analytics/app/login_refresh_proto.py (build_frame) + ../analytics/app/login_refresh_head.py
// (build_qqlive_head/build_qqlive_frame, đường ưu tiên dựng head từ session
// material, KHÔNG đọc template .bin). Dùng pb_codec để mã hoá field, giữ THỨ TỰ.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>

#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/model/session_material.h"

namespace tencent::trpc {

// Vật liệu phiên đã chuyển sang model/session_material.h
// (SessionMaterial) — nó là dữ liệu thuần, không thuộc tầng protocol.

// ../analytics/app/login_refresh_proto.py:190-201 — khung VBPB 16B (magic 0x0930).
std::string build_frame(const std::string& head_body, std::size_t head_len);

// ../analytics/app/login_refresh_head.py:161-210 — outer TRPC RequestProtocol + qqlive_head.
// ts_ms: nếu <=0 dùng now_ms (Python lấy session["ts_ms"] hoặc time.time()*1000).
std::string build_qqlive_head(const std::string& service, const std::string& method,
                              const model::SessionMaterial& s, int64_t protocol_id, uint64_t now_ms,
                              const OrderedKV& extra_trans_info = {});

// ../analytics/app/login_refresh_head.py:213-230 — build_qqlive_head + body rồi đóng khung.
std::string build_qqlive_frame(const std::string& service, const std::string& method,
                               const std::string& body, const model::SessionMaterial& s,
                               int64_t protocol_id, uint64_t now_ms,
                               const OrderedKV& extra_trans_info = {});

}  // namespace tencent::trpc
