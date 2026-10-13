// auth_command.cpp — Lệnh `auth`: lấy credential từ thiết bị, làm mới phiên,
// xem thông tin tài khoản.
//
// Toàn bộ nghiệp vụ nằm ở service::AccountService; file này chỉ chuyển tham số
// dòng lệnh thành yêu cầu và trình bày kết quả.
#include "../commands.h"
#include "../console.h"

#include "tencent/core/common/clock.h"
#include "tencent/core/service/account_service.h"

namespace tencent::cli {
namespace {

service::AccountServiceOptions service_options(const AuthOptions& options) {
  service::AccountServiceOptions service;
  service.timeout_sec = options.timeout_sec;
  return service;
}

// Độ dài trường bí mật — in số ký tự để kiểm tra "có/không" mà KHÔNG lộ giá trị.
int length_of(const std::string& value) { return static_cast<int>(value.size()); }

int run_dump(const AuthOptions& options) {
  service::DumpRequest request;
  request.use_device = options.use_device;
  request.device_id = options.device_id;
  request.package = options.package;
  request.from_prefs_file = options.from_prefs_file;
  request.skip_device_info = options.skip_device_info;
  request.set_default = options.set_default;
  request.output_path = options.output_path;

  const service::AccountService service(service_options(options));
  auto result = service.dump(request);
  if (!result) return report(result.error());

  const service::DumpOutcome& outcome = result.value();
  const account::Account& account = outcome.account;

  if (outcome.device_auto_selected) {
    print("Đã tự chọn thiết bị: " + outcome.selected_device_id);
  }
  if (!outcome.selected_device_id.empty()) {
    print(std::string("Helper: root=") + (outcome.root_granted ? "đã cấp" : "chưa cấp"));
  }
  for (const auto& warning : outcome.warnings) print_error("cảnh báo: " + warning);

  print("Đã ghi " + outcome.path);
  print("  vuid   : " + account.vuid + (outcome.became_default ? "  (mặc định)" : "") +
        (account.nickname.empty() ? "" : "  " + account.nickname) +
        "  nguồn=" + outcome.source);
  print("  thiết bị: " + (account.brand.empty() ? "?" : account.brand) + " / " +
        (account.model.empty() ? "?" : account.model) +
        "  os=" + (account.os_version.empty() ? "?" : account.os_version) +
        "  app=" + (account.app_ver.empty() ? "?" : account.app_ver) + " (" +
        (account.app_sub_build.empty() ? "?" : account.app_sub_build) + ")");
  print("  màn hình: " + std::to_string(account.screen_width) + "x" +
        std::to_string(account.screen_height) + " @" + std::to_string(account.screen_dpi) +
        " dpi");
  print("  A identity  : qimei36=" + std::to_string(length_of(account.qimei36)) +
        " guid=" + std::to_string(length_of(account.guid)) +
        " wx_app_id=" + std::to_string(length_of(account.wx_app_id)) +
        " wx_open_id=" + std::to_string(length_of(account.wx_open_id)));
  print("  B credential: vusession=" + std::to_string(length_of(account.vusession)) +
        " vurefresh=" + std::to_string(length_of(account.vurefresh)) +
        " access=" + std::to_string(length_of(account.wx_access_token)) +
        " refresh=" + std::to_string(length_of(account.wx_refresh_token)));
  print("  C session   : usid=" + std::to_string(length_of(account.usid)));
  if (!account.turing.k34_native_tail.empty()) {
    print("  Turing      : uid=" + std::to_string(account.turing.package_uid) +
          " user=" + std::to_string(account.turing.android_user_id) + " K26=" +
          (account.turing.k26_source.empty() ? "(không)" : account.turing.k26_source) +
          " CL=" + (account.turing.k34_cl.empty() ? "?" : account.turing.k34_cl) +
          " tail=" + account.turing.k34_native_tail);
  }
  return 0;
}

int run_refresh(const AuthOptions& options) {
  const service::AccountService service(service_options(options));
  auto account_id = service.resolve_account_id(options.account_id);
  if (!account_id) return report(account_id.error());

  auto loaded = service.load(account_id.value());
  if (!loaded) return report(loaded.error());
  print("# làm mới phiên  account=" + loaded.value().vuid +
        (loaded.value().nickname.empty() ? "" : "  " + loaded.value().nickname));
  print("  thiết bị=" + loaded.value().brand + "/" + loaded.value().model +
        " app=" + loaded.value().app_ver);

  auto result = service.refresh(account_id.value(), options.dry_run);
  if (!result) return report(result.error());

  const service::RefreshOutcome& outcome = result.value();
  if (options.dry_run) {
    print("framed_len=" + std::to_string(outcome.framed_hex.size() / 2));
    print("framed_hex=" + outcome.framed_hex);
    print("(dry-run: bỏ --dry-run để thực sự gửi đi)");
    return 0;
  }
  print("Làm mới thành công, đã cập nhật " + outcome.account_path);
  print("  hạn phiên mới: " +
        format_expiry(outcome.vusession_expire_ts, util::now_seconds()));
  return 0;
}

int run_info(const AuthOptions& options) {
  const service::AccountService service(service_options(options));
  auto account_id = service.resolve_account_id(options.account_id);
  if (!account_id) return report(account_id.error());

  auto result = service.info(account_id.value(), options.local_only, options.allow_refresh);
  if (!result) return report(result.error());

  const service::AccountInfo& info = result.value();
  const account::Account& account = info.account;
  const int64_t now = util::now_seconds();

  if (info.identity_recovered) print("Đã khôi phục QIMEI/GUID còn thiếu.");
  print("# Tài khoản " + account.vuid);
  print("  biệt danh : " + (account.nickname.empty() ? "(không có)" : account.nickname));
  print("  ảnh đại diện: " + (account.head_url.empty() ? "(không có)" : account.head_url));
  print("  thiết bị  : " + account.brand + " / " + account.model + "  (os " +
        account.os_version + ", app " + account.app_ver + ")");
  print("  qimei36   : " + account.qimei36);
  print("  guid      : " + account.guid);
  print("  phiên     : " + format_expiry(account.vusession_expire_ts, now) +
        (info.refreshed ? "  (vừa tự làm mới)" : ""));

  for (const auto& warning : info.warnings) print_error("cảnh báo: " + warning);
  if (!info.queried_server) return 0;

  print("");
  print("# VIP");
  print(info.vip_summary);
  if (!info.diamond_balance.empty()) {
    print("");
    print("# Tài sản");
    print("  kim cương : " + info.diamond_balance);
  }
  return 0;
}

int run_delete(const AuthOptions& options) {
  const service::AccountService service(service_options(options));
  auto result = service.remove(options.account_id);
  if (!result) return report(result.error());

  const service::DeleteOutcome& outcome = result.value();
  print("Đã xoá account " + outcome.account_id);
  if (outcome.removed_bundle) print("  bundle : " + outcome.account_path);
  if (outcome.removed_legacy) print("  legacy : " + outcome.legacy_path);
  print("  mục đã xoá: " + std::to_string(outcome.removed_entries));
  if (outcome.cleared_default) print("  đã bỏ account mặc định");
  return 0;
}

}  // namespace

int run_auth(const AuthOptions& options) {
  switch (options.mode) {
    case AuthOptions::Mode::kDelete:
      return run_delete(options);
    case AuthOptions::Mode::kRefresh:
      return run_refresh(options);
    case AuthOptions::Mode::kInfo:
      return run_info(options);
    case AuthOptions::Mode::kDump:
      break;
  }
  return run_dump(options);
}

}  // namespace tencent::cli
