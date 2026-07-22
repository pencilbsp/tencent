// request_context.cpp — Xem include/tencent/core/protocol/playback/request_context.h.
//
// Chuyển từ build_getvinfo_context() trong tencent_cli_main.cpp cũ: việc ghép
// hằng số giao thức với dữ liệu account là NGHIỆP VỤ, không phải việc của CLI.
#include "tencent/core/protocol/playback/request_context.h"

#include "tencent/core/common/ordered_kv.h"
#include "tencent/core/protocol/playback/constants.h"

namespace tencent::playback {

GetvinfoContext build_request_context(const account::Account& account, int64_t now_ms) {
  GetvinfoContext context;

  // ── Phần PER-BUILD: lấy từ bảng hằng số (dump một lần cho mỗi bản APK) ─────
  context.preamble_hex = constants::kCkeyPreambleHex;
  context.raw_bitmap = constants::kCkeyRawBitmap;
  context.raw_ints = {0, 0, 0};
  context.trailing_hex = constants::kCkeyTrailingHex;
  context.query_params = constants::request_param_defaults();

  // ── Phần PER-DEVICE/PER-USER: đi theo account ─────────────────────────────
  context.guid = account.guid;
  context.qimei36 = account.qimei36;
  context.device_model = account.model;

  set_value(context.query_params, "guid", account.guid);
  set_value(context.query_params, "qimei", account.qimei36);
  set_value(context.query_params, "abuserid", account.qimei36);
  set_value(context.query_params, "userid", account.vuid);
  set_value(context.query_params, "openid", account.wx_open_id);
  set_value(context.query_params, "appVer", account.app_ver);
  set_value(context.query_params, "manufacturer", account.manufacturer);
  set_value(context.query_params, "devicemodel", account.model);
  set_value(context.query_params, "osver", account.os_version);
  if (account.screen_width > 0 && account.screen_height > 0) {
    set_value(context.query_params, "screeninfo",
              std::to_string(account.screen_width) + "*" +
                  std::to_string(account.screen_height));
  }

  // Cookie được dựng LẠI mỗi request (có mốc thời gian bên trong), không sao
  // chép từ bản dump.
  context.cookies = account::build_login_cookie(account, now_ms);
  return context;
}

}  // namespace tencent::playback
