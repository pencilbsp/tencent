// login_refresh.cpp — Xem login_refresh.h. Port byte-exact:
//   ../analytics/app/login_refresh_proto.py  (RefreshRequest proto3 + omit-identity)
//   ../analytics/app/login_refresh_response.py (unframe + parse RefreshResponse)
//   ../analytics/app/login_refresh.py:157-216 (build_refresh_frame, nhánh build_qqlive_frame)
#include "tencent/core/protocol/auth/login_refresh.h"

#include <stdexcept>
#include <vector>

#include "tencent/core/protocol/auth/login_sign.h"
#include "tencent/core/codec/protobuf.h"
#include "tencent/core/protocol/trpc/head.h"

namespace tencent::auth {

namespace pb = tencent::pb;

namespace {

// proto3 OMIT_IDENTITY helpers (../analytics/app/login_refresh_proto.py:61-83).
std::string f_int64(int field, int64_t value) {
  if (value == 0) return "";
  return pb::Field::varint(field, static_cast<uint64_t>(value)).encode();
}
std::string f_enum(int field, int value) {
  if (value == 0) return "";
  return pb::Field::varint(field, static_cast<uint64_t>(value)).encode();
}
std::string f_string(int field, const std::string& value) {
  if (value.empty()) return "";
  return pb::Field::bytes(field, value).encode();
}
std::string f_message(int field, const std::string& body) {
  if (body.empty()) return "";
  return pb::Field::bytes(field, body).encode();
}

// ThirdInfo.encode (../analytics/app/login_refresh_proto.py:96-103).
std::string encode_third_info(int third_type, const std::string& app_id,
                              const std::string& open_id, const std::string& access_token,
                              const std::string& refresh_token) {
  return f_enum(1, third_type) + f_string(2, app_id) + f_string(3, open_id) +
         f_string(4, access_token) + f_string(5, refresh_token);
}

// SignInfo.encode (../analytics/app/login_refresh_proto.py:113-119).
std::string encode_sign_info(const std::string& qimei36, const std::string& h38,
                             const std::string& original_data, const std::string& sign) {
  return f_string(1, qimei36) + f_string(2, h38) + f_string(3, original_data) +
         f_string(4, sign);
}

std::string pb_str(const std::vector<pb::Field>& fields, int num) {
  const pb::Field* f = pb::get_first(fields, num);
  return (f && f->wire == 2) ? f->data : std::string();
}
int64_t pb_int(const std::vector<pb::Field>& fields, int num) {
  const pb::Field* f = pb::get_first(fields, num);
  return (f && f->wire == 0) ? static_cast<int64_t>(f->intval) : 0;
}

}  // namespace

// ── build_refresh_request (../analytics/app/login_refresh_proto.py:142-176) ─────────────
std::string build_refresh_request(int64_t vuid, const std::string& vusession,
                                  const std::string& vurefresh,
                                  const std::string& wx_app_id,
                                  const std::string& wx_open_id,
                                  const std::string& wx_access_token,
                                  const std::string& wx_refresh_token,
                                  const std::string& qimei36,
                                  const std::string& original_data,
                                  const std::string& sign, int third_type,
                                  const std::string& h38,
                                  bool include_third_info) {
  std::string third;
  if (include_third_info) {
    third = encode_third_info(third_type, wx_app_id, wx_open_id,
                              wx_access_token, wx_refresh_token);
  }
  // sign_info chỉ dựng khi có dữ liệu (proto3, login_refresh_proto.py:169-172).
  std::string sign_info;
  bool has_sign = !qimei36.empty() || !sign.empty() || !original_data.empty() || !h38.empty();
  if (has_sign) sign_info = encode_sign_info(qimei36, h38, original_data, sign);

  std::string out;
  out += f_int64(1, vuid);
  out += f_string(2, vusession);
  out += f_string(3, vurefresh);
  // Phone account old-video refresh uses third_info=null in LoginUtils.b(account).
  out += f_message(4, third);
  if (has_sign) out += f_message(5, sign_info);
  return out;
}

// Dựng frame từ Session đã có đầy đủ device/app context.
std::string build_refresh_frame_from_session(const model::SessionMaterial& base, uint64_t now_ms,
                                             uint32_t rnd, uint32_t usec,
                                             const std::string& sign) {
  std::string guid = base.get("guid");
  std::string vurefresh = base.get("vurefresh");
  int64_t vuid = 0;
  try { vuid = std::stoll(base.get("vuid", "0")); } catch (...) {}

  std::string original_data =  build_original_data(guid, vurefresh, now_ms, rnd);
  std::string sig = sign.empty() ? make_sign(original_data, usec) : sign;
  const bool has_third_identity =
      !base.get("wx_app_id").empty() || !base.get("wx_open_id").empty() ||
      !base.get("wx_access_token").empty() || !base.get("wx_refresh_token").empty();
  const bool include_third_info =
      base.get_int("login_type", -1) != 2 && has_third_identity;

  std::string body = build_refresh_request(
      vuid, base.get("vusession"), vurefresh, base.get("wx_app_id"), base.get("wx_open_id"),
      base.get("wx_access_token"), base.get("wx_refresh_token"), base.get("qimei36"),
      original_data, sig, kThirdTypeWX, "", include_third_info);

  model::SessionMaterial s = base;            // giữ device + identity; chỉ set ts_ms.
  s.kv["ts_ms"] = std::to_string(now_ms);
  return trpc::build_qqlive_frame(kRefreshService, kRefreshMethod, body, s,
                                  kRefreshProtocolId, now_ms);
}

// ── unframe_response (../analytics/app/login_refresh_response.py:42-53) ─────────────────
void unframe_response(const std::string& resp, std::string& head, std::string& body) {
  if (resp.size() < 16) throw std::runtime_error("response too short");
  auto be16 = [&](std::size_t o) {
    return (static_cast<uint16_t>(static_cast<uint8_t>(resp[o])) << 8) |
           static_cast<uint8_t>(resp[o + 1]);
  };
  auto be32 = [&](std::size_t o) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(resp[o])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(resp[o + 1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(resp[o + 2])) << 8) |
           static_cast<uint8_t>(resp[o + 3]);
  };
  uint16_t magic = be16(0);
  if (magic != 0x0930 && magic != 0x2352)
    throw std::runtime_error("bad response magic");
  uint32_t total_len = be32(4);
  uint16_t head_len = be16(8);
  if (total_len > resp.size()) total_len = static_cast<uint32_t>(resp.size());
  std::string payload = resp.substr(16, total_len - 16);
  head = payload.substr(0, head_len);
  body = payload.substr(head_len);
}

// parse_trpc_ret (../analytics/app/login_refresh_response.py:87-101): f4=ret, f5=func_ret.
static bool parse_trpc_ret(const std::string& head, int64_t& out) {
  std::vector<pb::Field> fields;
  try {
    fields = pb::parse(head);
  } catch (...) {
    return false;
  }
  for (int num : {4, 5}) {
    const pb::Field* f = pb::get_first(fields, num);
    if (f && f->wire == 0) {
      out = static_cast<int64_t>(f->intval);
      return true;
    }
  }
  return false;
}

// parse_refresh_response (../analytics/app/login_refresh_response.py:104-121).
RefreshResult parse_refresh_response(const std::string& resp) {
  std::string head, body;
  unframe_response(resp, head, body);
  std::vector<pb::Field> fields = pb::parse(body);

  RefreshResult r;
  r.vuid = pb_int(fields, 1);
  r.vusession = pb_str(fields, 2);
  r.vusession_expire_timestamp = pb_int(fields, 3);
  r.vurefresh = pb_str(fields, 4);
  r.has_trpc_ret = parse_trpc_ret(head, r.trpc_ret);

  const pb::Field* third = pb::get_first(fields, 5);
  if (third && third->wire == 2) {
    std::vector<pb::Field> tf = pb::parse(third->data);
    r.third_app_id = pb_str(tf, 2);
    r.third_open_id = pb_str(tf, 3);
    r.third_access_token = pb_str(tf, 4);
    r.third_refresh_token = pb_str(tf, 5);
  }
  return r;
}

}  // namespace tencent::auth
