// trpc_head.cpp — Port byte-exact ../analytics/app/login_refresh_proto.py +
// ../analytics/app/login_refresh_head.py. Xem trpc_head.h.
#include "tencent/core/protocol/trpc/head.h"

#include <optional>
#include <stdexcept>
#include <vector>

#include "tencent/core/codec/protobuf.h"

namespace tencent::trpc {

namespace pb = tencent::pb;

// Các phương thức của model::SessionMaterial (nay là model::SessionMaterial) nằm ở
// src/model/session_material.cpp.

static const std::string kQqliveHeadKey = "qqlive_head";

static void require_device_context(const model::SessionMaterial& s) {
  const char* strings[] = {
      "model", "brand", "os_version", "app_ver",
      "app_build", "app_sub_build", "platform",
  };
  for (const char* key : strings) {
    if (s.get(key).empty())
      throw std::invalid_argument(std::string("missing account field for TRPC head: ") + key);
  }
  const char* positive_ints[] = {
      "android_api_level", "screen_width", "screen_height", "screen_dpi",
  };
  for (const char* key : positive_ints) {
    try {
      if (s.get(key).empty() || s.get_int(key, 0) <= 0)
        throw std::invalid_argument(std::string("invalid account field for TRPC head: ") + key);
    } catch (const std::invalid_argument&) {
      throw;
    } catch (...) {
      throw std::invalid_argument(std::string("invalid account field for TRPC head: ") + key);
    }
  }
}

// ── Helper field (../analytics/app/login_refresh_head.py:43-66) ────────────────────────
// _str_field: None nếu rỗng.
static std::optional<pb::Field> str_field(int num, const std::string& v) {
  if (v.empty()) return std::nullopt;
  return pb::Field::bytes(num, v);
}
// _var_field: None nếu value None/"" — ở đây luôn nhận int nên luôn phát.
static pb::Field var_field(int num, long v) {
  return pb::Field::varint(num, static_cast<uint64_t>(v));
}
// _msg_field: serialize các field khác null thành 1 message.
static pb::Field msg_field(int num, const std::vector<std::optional<pb::Field>>& fields) {
  std::vector<pb::Field> kept;
  for (const auto& f : fields)
    if (f) kept.push_back(*f);
  return pb::Field::bytes(num, pb::serialize(kept));
}
// _kv: f15 KV {f1=key, f2=value}; None nếu value rỗng.
static std::optional<pb::Field> kv_field(const std::string& key, const std::string& value) {
  if (value.empty()) return std::nullopt;
  return msg_field(15, {pb::Field::bytes(1, key), pb::Field::bytes(2, value)});
}

// ── Các block (../analytics/app/login_refresh_head.py:77-158) ──────────────────────────
// _build_auth_block (f5[0]).
static pb::Field build_auth_block(const model::SessionMaterial& s) {
  return msg_field(5, {
      var_field(2, s.get_int("login_type", 9)),
      str_field(3, s.pick({"vuid"})),
      str_field(4, s.pick({"vusession"})),
  });
}

// _build_app_block (f5[1]); None nếu không có field 1/3/4.
static std::optional<pb::Field> build_app_block(const model::SessionMaterial& s) {
  std::string app_id = s.pick({"wx_app_id", "app_id"});
  std::string open_id = s.pick({"wx_open_id", "open_id"});
  std::string access = s.pick({"wx_access_token", "access_token"});
  if (app_id.empty() && open_id.empty() && access.empty()) return std::nullopt;
  return msg_field(5, {
      str_field(1, app_id),
      var_field(2, s.get_int("third_type", 100)),
      str_field(3, open_id),
      str_field(4, access),
      var_field(5, s.get_int("third_enabled", 1)),
  });
}

// _build_device_block (f6).
static pb::Field build_device_block(const model::SessionMaterial& s) {
  std::string model = s.pick({"model"});
  std::string qimei36 = s.pick({"qimei36", "qimei"});
  return msg_field(6, {
      var_field(1, s.get_int("screen_width", 0)),
      var_field(2, s.get_int("screen_height", 0)),
      var_field(3, s.get_int("screen_dpi", 0)),
      str_field(7, s.pick({"omgid"})),
      str_field(8, s.pick({"device_id_short"})),
      str_field(9, model),
      var_field(10, s.get_int("is_phone", 1)),
      str_field(11, s.pick({"guid"})),
      str_field(14, s.get("brand")),
      str_field(15, qimei36),
  });
}

// _build_version_block (f7).
static pb::Field build_version_block(const model::SessionMaterial& s) {
  std::string model = s.pick({"model"});
  std::string os_version = s.get("os_version");
  std::string android_api_level = s.get("android_api_level");
  return msg_field(7, {
      str_field(1, s.get("app_ver")),
      str_field(2, s.get("app_build")),
      var_field(3, s.get_int("platform_type", 3)),
      str_field(4, model + "," + android_api_level + "," + os_version),
      str_field(6, s.get("app_sub_build")),
      str_field(7, s.get("platform")),
  });
}

// _build_net_block (f8).
static pb::Field build_net_block(const model::SessionMaterial& s) {
  std::vector<std::optional<pb::Field>> fields = {var_field(1, s.get_int("network_type", 3))};
  std::string ip = s.pick({"ip"});
  if (!ip.empty()) {
    fields.push_back(var_field(2, s.get_int("ip_type", 2)));
    fields.push_back(str_field(3, ip));
  }
  return msg_field(8, fields);
}

// _build_head_kvs (f15 lặp) — ../analytics/app/login_refresh_head.py:140-158.
static std::vector<std::optional<pb::Field>> build_head_kvs(const model::SessionMaterial& s) {
  std::string qimei36 = s.pick({"qimei36", "qimei"});
  std::string os_version = s.get("os_version");
  std::vector<std::pair<std::string, std::string>> pairs = {
      {"device_model_type", s.pick({"device_model_type"}, "phone")},
      {"skp_mode", s.pick({"skp_mode"}, "0")},
      {"callType", s.pick({"call_type"}, "self")},
      {"usid", s.pick({"usid"})},
      {"qimei", qimei36},
      {"access_token", s.pick({"wx_access_token", "access_token"})},
      {"refresh_token", s.pick({"wx_refresh_token", "refresh_token"})},
      {"QIMEI36", qimei36},
      {"os_version", os_version},
      {"turing_ticket", s.pick({"turing_ticket"})},
      {"uskey", s.pick({"uskey"})},
  };
  std::vector<std::optional<pb::Field>> out;
  for (const auto& [k, v] : pairs) {
    auto f = kv_field(k, v);
    if (f) out.push_back(f);
  }
  return out;
}

// ── build_frame (../analytics/app/login_refresh_proto.py:190-201) ──────────────────────
static void put_be16(std::string& o, uint16_t v) {
  o.push_back(static_cast<char>((v >> 8) & 0xFF));
  o.push_back(static_cast<char>(v & 0xFF));
}
static void put_be32(std::string& o, uint32_t v) {
  o.push_back(static_cast<char>((v >> 24) & 0xFF));
  o.push_back(static_cast<char>((v >> 16) & 0xFF));
  o.push_back(static_cast<char>((v >> 8) & 0xFF));
  o.push_back(static_cast<char>(v & 0xFF));
}

std::string build_frame(const std::string& head_body, std::size_t head_len) {
  const uint32_t total = static_cast<uint32_t>(head_body.size() + 16);
  std::string hdr;
  put_be16(hdr, 0x0930);          // FRAME_MAGIC_REQUEST
  hdr.append(2, '\0');            // 00 00
  put_be32(hdr, total);           // totalLen
  put_be16(hdr, static_cast<uint16_t>(head_len));  // headLen
  hdr.append(2, '\0');            // 00 00
  hdr.append(4, '\0');            // 00 00 00 00
  return hdr + head_body;
}

// ── build_qqlive_head (../analytics/app/login_refresh_head.py:161-210) ─────────────────
std::string build_qqlive_head(const std::string& service, const std::string& method,
                              const model::SessionMaterial& s, int64_t protocol_id, uint64_t now_ms,
                              const OrderedKV& extra_trans_info) {
  require_device_context(s);
  long ts_ms = s.get_int("ts_ms", 0);
  if (ts_ms <= 0) ts_ms = static_cast<long>(now_ms);

  std::string guid = s.pick({"guid"});

  std::vector<std::optional<pb::Field>> qqlive_head = {
      pb::Field::varint(1, static_cast<uint64_t>(protocol_id)),
      pb::Field::bytes(2, service),
      pb::Field::bytes(3, method),
      build_auth_block(s),
      build_app_block(s),
      build_device_block(s),
      build_version_block(s),
      build_net_block(s),
      pb::Field::bytes(9, ""),
      pb::Field::bytes(10, ""),
      pb::Field::bytes(11, ""),
      str_field(13, guid.empty() ? std::string()
                                 : guid + "@" + std::to_string(ts_ms) + "@" +
                                       std::to_string(protocol_id)),
  };
  for (auto& f : build_head_kvs(s)) qqlive_head.push_back(f);
  qqlive_head.push_back(pb::Field::bytes(18, ""));
  qqlive_head.push_back(msg_field(19, {pb::Field::varint(1, static_cast<uint64_t>(ts_ms))}));

  std::vector<pb::Field> head_kept;
  for (const auto& f : qqlive_head)
    if (f) head_kept.push_back(*f);

  pb::Field trans = msg_field(9, {pb::Field::bytes(1, kQqliveHeadKey),
                                  pb::Field::bytes(2, pb::serialize(head_kept))});

  std::vector<pb::Field> top = {
      pb::Field::varint(3, static_cast<uint64_t>(protocol_id)),
      pb::Field::bytes(6, service),
      pb::Field::bytes(7, method),
      trans,
  };
  for (const auto& [key, value] : extra_trans_info)
    top.push_back(msg_field(9, {pb::Field::bytes(1, key), pb::Field::bytes(2, value)}));
  return pb::serialize(top);
}

// ../analytics/app/login_refresh_head.py:213-230
std::string build_qqlive_frame(const std::string& service, const std::string& method,
                               const std::string& body, const model::SessionMaterial& s,
                               int64_t protocol_id, uint64_t now_ms,
                               const OrderedKV& extra_trans_info) {
  std::string head = build_qqlive_head(service, method, s, protocol_id, now_ms, extra_trans_info);
  return build_frame(head + body, head.size());
}

}  // namespace tencent::trpc
