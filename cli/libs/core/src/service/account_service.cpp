// account_service.cpp — Xem include/tencent/core/service/account_service.h.
//
// Toàn bộ luồng nghiệp vụ ở đây trước kia nằm trong tencent_cli_main.cpp
// (cmd_list / cmd_auth_dump / cmd_auth_refresh / cmd_auth_info + hai hàm
// refresh_and_persist, ensure_fresh_vusession). Chuyển vào lõi để GUI dùng lại
// được y hệt; điểm khác duy nhất là KHÔNG in gì — kết quả/cảnh báo trả về qua
// cấu trúc dữ liệu.
#include "tencent/core/service/account_service.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "tencent/core/common/clock.h"
#include "tencent/core/common/hex.h"
#include "tencent/core/common/json.h"
#include "tencent/core/common/logging.h"
#include "tencent/core/common/strings.h"
#include "tencent/core/device/adb_bridge.h"
#include "tencent/core/device/helper_probe.h"
#include "tencent/core/net/http_client.h"
#include "tencent/core/protocol/auth/login_refresh.h"
#include "tencent/core/protocol/auth/user_center.h"
#include "tencent/core/protocol/auth/vip_info.h"
#include "tencent/core/protocol/identity/guid_client.h"
#include "tencent/core/protocol/identity/qimei_client.h"
#include "tencent/core/protocol/trpc/head.h"
#include "tencent/core/service/error.h"
#include "tencent/core/storage/account_store.h"
#include "tencent/core/storage/paths.h"
#include "tencent/platform/filesystem.h"

namespace tencent::service {
namespace fs = std::filesystem;
namespace {

// Điểm cuối TRPC dùng chung cho refresh / VIP / User Center.
constexpr const char* kTrpcEndpoint = "https://a.video.qq.com/";

// Header bắt buộc của mọi request TRPC: uqm = qimei36, loginv = vuid.
OrderedKV trpc_headers(const account::Account& account) {
  return {{"uqm", account.qimei36},
          {"loginv", account.vuid},
          {"Content-Encoding", "gzip"},
          {"Content-Type", "application/octet-stream"},
          {"Accept-Encoding", "gzip"},
          {"Connection", "close"}};
}

// Gửi một khung TRPC đã dựng sẵn: gzip → POST → gunzip → tách head/body.
// Ném std::runtime_error kèm mô tả nếu HTTP lỗi.
void post_trpc_frame(const std::string& framed, const account::Account& account,
                     long timeout_sec, std::string& out_head, std::string& out_body) {
  const auto [status, raw] =
      http::http_post(kTrpcEndpoint, trpc_headers(account), http::gzip_compress(framed),
                      timeout_sec);
  if (status != 200) throw std::runtime_error("HTTP " + std::to_string(status));
  auth::unframe_response(http::maybe_gunzip(raw), out_head, out_body);
}

}  // namespace

// ── Liệt kê ──────────────────────────────────────────────────────────────────

Result<std::vector<AccountSummary>> AccountService::list() const {
  return guard<std::vector<AccountSummary>>(
      ErrorCode::kIo, "không đọc được thư mục account", [&] {
        std::vector<AccountSummary> summaries;
        const std::string dir = storage::accounts_dir();
        if (!platform::directory_exists(dir)) return summaries;

        const std::string default_id = storage::default_account();
        for (const auto& entry : fs::directory_iterator(dir)) {
          std::string candidate;
          if (entry.is_directory() && fs::is_regular_file(entry.path() / "auth.json")) {
            candidate = entry.path().string();
          } else if (entry.is_regular_file() && entry.path().extension() == ".json") {
            // File monolith cũ: bỏ qua nếu đã có bundle mới cùng vuid.
            const std::string vuid = entry.path().stem().string();
            if (fs::is_regular_file(fs::path(storage::account_path(vuid)) / "auth.json")) {
              continue;
            }
            candidate = entry.path().string();
          } else {
            continue;
          }

          AccountSummary summary;
          summary.path = candidate;
          try {
            const account::Account account = storage::load_account(candidate);
            summary.vuid = account.vuid;
            summary.nickname = account.nickname;
            summary.model = account.model;
            summary.app_ver = account.app_ver;
            summary.qimei36 = account.qimei36;
            summary.is_default = !default_id.empty() && account.vuid == default_id;
          } catch (const std::exception&) {
            summary.readable = false;
            summary.vuid = fs::path(candidate).filename().string();
          }
          summaries.push_back(std::move(summary));
        }
        return summaries;
      });
}

Result<std::string> AccountService::resolve_account_id(const std::string& requested) const {
  if (!requested.empty()) return requested;
  const std::string default_id = storage::default_account();
  if (default_id.empty()) {
    return make_error(ErrorCode::kNotFound,
                      "chưa có account nào được chọn",
                      "chạy `tencent-cli auth --adb` trước, hoặc truyền --account");
  }
  return default_id;
}

std::string AccountService::path_for(const std::string& account_id) const {
  return storage::account_path(account_id);
}

std::string AccountService::accounts_directory() const { return storage::accounts_dir(); }

Result<account::Account> AccountService::load(const std::string& account_id) const {
  const std::string path = path_for(account_id);
  if (!storage::account_exists(path)) {
    return make_error(ErrorCode::kNotFound, "không tìm thấy account " + account_id, path);
  }
  return guard<account::Account>(ErrorCode::kIo, "không nạp được account " + account_id,
                                 [&] { return storage::load_account(path); });
}

Result<DeleteOutcome> AccountService::remove(const std::string& account_id) const {
  if (account_id.empty()) {
    return make_error(ErrorCode::kInvalidArgument, "cần truyền <vuid> để xoá account");
  }
  const bool safe_id = std::all_of(account_id.begin(), account_id.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '_' || ch == '-';
  });
  if (!safe_id || account_id == "." || account_id == "..") {
    return make_error(ErrorCode::kInvalidArgument,
                      "vuid không hợp lệ",
                      "chỉ nhận chữ/số, gạch dưới hoặc gạch nối");
  }

  const fs::path bundle = storage::account_path(account_id);
  const fs::path legacy = storage::legacy_account_path(account_id);
  const bool has_bundle = storage::account_exists(bundle.string());
  std::error_code exists_ec;
  const bool has_legacy = fs::is_regular_file(legacy, exists_ec);
  if (!has_bundle && !has_legacy) {
    return make_error(ErrorCode::kNotFound, "không tìm thấy account " + account_id);
  }

  return guard<DeleteOutcome>(ErrorCode::kIo, "không xoá được account", [&] {
    DeleteOutcome outcome;
    outcome.account_id = account_id;
    outcome.account_path = bundle.string();
    outcome.legacy_path = legacy.string();

    std::error_code ec;
    if (has_bundle) {
      outcome.removed_entries += static_cast<uint64_t>(fs::remove_all(bundle, ec));
      if (ec) throw std::runtime_error(bundle.string() + ": " + ec.message());
      outcome.removed_bundle = true;
    }
    if (has_legacy) {
      ec.clear();
      const bool removed = fs::remove(legacy, ec);
      if (ec) throw std::runtime_error(legacy.string() + ": " + ec.message());
      if (removed) {
        outcome.removed_entries += 1;
        outcome.removed_legacy = true;
      }
    }

    if (storage::default_account() == account_id) {
      storage::set_default_account("");
      outcome.cleared_default = true;
    }
    return outcome;
  });
}

// ── Khôi phục identity ───────────────────────────────────────────────────────

Status AccountService::recover_identity(account::Account& account) const {
  // Thứ tự phụ thuộc: fingerprint thô → qimei36 (REGISTER) → guid (StGuidGet).
  if (account.model.empty() || account.manufacturer.empty() || account.os_version.empty() ||
      account.android_api_level.empty() || account.app_ver.empty() ||
      account.app_sub_build.empty() || account.screen_width <= 0 ||
      account.screen_height <= 0 || account.screen_dpi <= 0) {
    return make_error(ErrorCode::kInvalidArgument,
                      "vân tay thiết bị thiếu trường bắt buộc để khôi phục identity");
  }

  return guard_status(ErrorCode::kNetwork, "khôi phục identity thất bại", [&] {
    if (account.qimei36.empty()) {
      if (account.raw_fingerprint_json.empty()) {
        throw std::runtime_error(
            "thiếu qimei36 và không có fingerprint thô để đăng ký lại");
      }
      const qimei::DeviceProfile profile =
          qimei::profile_from_raw_json(account.raw_fingerprint_json);
      account.qimei36 = qimei::register_device(profile, options_.timeout_sec).qimei36;
    }
    if (account.guid.empty()) {
      guid::Context context;
      context.qimei36 = account.qimei36;
      context.usid = account.usid;
      context.version_name = account.app_ver;
      context.version_code = account.app_sub_build;
      context.channel_id = "20563";
      context.model = account.model;
      context.manufacturer = account.manufacturer;
      context.os_version = account.os_version;
      context.screen_width = account.screen_width;
      context.screen_height = account.screen_height;
      context.screen_dpi = account.screen_dpi;
      account.guid = guid::recover(context, options_.timeout_sec).guid;
    }
  });
}

// ── Làm mới vusession ────────────────────────────────────────────────────────

Result<RefreshOutcome> AccountService::refresh(const std::string& account_id,
                                               bool dry_run) const {
  auto loaded = load(account_id);
  if (!loaded) return loaded.error();
  account::Account account = loaded.value();
  const std::string path = path_for(account_id);
  const bool has_wechat_identity =
      !account.wx_app_id.empty() || !account.wx_open_id.empty() ||
      !account.wx_access_token.empty() || !account.wx_refresh_token.empty();
  const bool phone_refresh = account.login_type == 2 || !has_wechat_identity;

  if (dry_run) {
    // Chỉ dựng khung request để đối chiếu byte-exact; không chạm mạng.
    return guard<RefreshOutcome>(ErrorCode::kInternal, "không dựng được khung refresh", [&] {
      const util::RequestClock clock = util::read_request_clock();
      const std::string framed = auth::build_refresh_frame_from_session(
          account.to_session(), clock.now_ms, clock.rnd, clock.usec);
      RefreshOutcome outcome;
      outcome.account_path = path;
      outcome.framed_hex = util::hex_encode(framed);
      return outcome;
    });
  }

  bool performed = false;
  auto status = [&]() -> Status {
    if (account.qimei36.empty() || account.guid.empty()) {
      if (const Status recovered = recover_identity(account); !recovered) {
        return recovered.error();
      }
    }
    if (const std::string missing = account::missing_required(account); !missing.empty()) {
      return make_error(ErrorCode::kInvalidArgument, "account thiếu trường bắt buộc", missing);
    }
    return Status();
  }();
  if (!status) return status.error();

  auto sent = guard<auth::RefreshResult>(ErrorCode::kNetwork, "gọi refresh thất bại", [&] {
    const util::RequestClock clock = util::read_request_clock();
    const std::string framed = auth::build_refresh_frame_from_session(
        account.to_session(), clock.now_ms, clock.rnd, clock.usec);
    const auto [http_status, raw] =
        http::http_post(kTrpcEndpoint, trpc_headers(account), http::gzip_compress(framed),
                        options_.timeout_sec);
    if (http_status != 200) throw std::runtime_error("HTTP " + std::to_string(http_status));
    return auth::parse_refresh_response(http::maybe_gunzip(raw));
  });
  if (!sent) return sent.error();

  const auth::RefreshResult& result = sent.value();
  if (result.has_trpc_ret && result.trpc_ret != 0) {
    const std::string detail =
        phone_refresh
            ? "phone old-video refresh đã gửi third_info=null; nếu vẫn bị từ chối "
              "thì account này cần nhánh AuthCenter RefreshToken hoặc dump metadata mới"
        : has_wechat_identity
            ? "vurefresh hoặc token WeChat đã hết hạn — hãy đăng nhập lại WeChat "
              "trên thiết bị rồi dump lại"
            : "máy chủ từ chối request refresh";
    return make_error(ErrorCode::kAuthExpired,
                      "máy chủ từ chối làm mới phiên (trpc_ret=" +
                          std::to_string(result.trpc_ret) + ")",
                      detail);
  }
  if (result.vusession.empty()) {
    return make_error(ErrorCode::kProtocol, "phản hồi không chứa vusession mới");
  }

  // Ghi đè credential: sao lưu bản cũ trước (mất vurefresh là mất tài khoản).
  const int64_t now = util::now_seconds();
  try {
    storage::save_account(storage::backup_path(account.vuid, now), account);
  } catch (const std::exception& e) {
    log_warn(std::string("không sao lưu được account trước khi làm mới: ") + e.what());
  }
  account.vusession = result.vusession;
  if (!result.vurefresh.empty()) account.vurefresh = result.vurefresh;
  if (!result.third_access_token.empty()) account.wx_access_token = result.third_access_token;
  if (!result.third_refresh_token.empty()) {
    account.wx_refresh_token = result.third_refresh_token;
  }
  if (result.vusession_expire_timestamp) {
    account.vusession_expire_ts = result.vusession_expire_timestamp;
  }
  account.updated_at = now;

  auto saved = guard_status(ErrorCode::kIo, "không ghi được account", [&] {
    storage::save_account(path, account);
  });
  if (!saved) return saved.error();
  performed = true;

  RefreshOutcome outcome;
  outcome.performed = performed;
  outcome.vusession_expire_ts = account.vusession_expire_ts;
  outcome.account_path = path;
  return outcome;
}

Result<bool> AccountService::ensure_fresh_session(account::Account& account,
                                                  const std::string& account_path) const {
  const int64_t now = util::now_seconds();
  const bool still_valid = !account.vusession.empty() &&
                           account.vusession_expire_ts > now + options_.refresh_skew_sec;
  if (still_valid) return false;

  auto refreshed = refresh(account.vuid.empty()
                               ? fs::path(account_path).filename().string()
                               : account.vuid,
                           /*dry_run=*/false);
  if (!refreshed) return refreshed.error();

  // Nạp lại bản vừa ghi để `account` phản ánh đúng những gì trên đĩa.
  auto reloaded = guard<account::Account>(ErrorCode::kIo, "không nạp lại được account",
                                          [&] { return storage::load_account(account_path); });
  if (!reloaded) return reloaded.error();
  account = reloaded.value();
  return true;
}

// ── Lấy dữ liệu từ thiết bị ──────────────────────────────────────────────────

Result<DumpOutcome> AccountService::dump(const DumpRequest& request) const {
  const bool offline = !request.from_prefs_file.empty();
  if (!request.use_device && !offline) {
    return make_error(ErrorCode::kInvalidArgument,
                      "cần --adb (đọc từ thiết bị) hoặc --from-prefs <file> (ngoại tuyến)");
  }

  DumpOutcome outcome;
  account::Account& account = outcome.account;
  turing::HelperSnapshot helper;
  util::Json raw = util::Json::object();
  std::string device_id = request.device_id;
  bool reused_existing = false;

  // 1) Chọn thiết bị khi người dùng không chỉ định.
  if (!offline && device_id.empty()) {
    const std::vector<std::string> devices = adb::list_devices();
    if (devices.empty()) {
      return make_error(ErrorCode::kDeviceUnavailable,
                        "không thấy thiết bị adb nào đang online",
                        "cắm thiết bị hoặc kiểm tra `adb devices`");
    }
    if (devices.size() > 1) {
      return make_error(ErrorCode::kInvalidArgument,
                        "có " + std::to_string(devices.size()) +
                            " thiết bị; hãy chỉ định `--adb <device_id>`",
                        util::join(devices, ", "));
    }
    device_id = devices[0];
    outcome.device_auto_selected = true;
  }
  outcome.selected_device_id = device_id;

  // 2) Lấy dữ liệu thô: từ file (ngoại tuyến) hoặc từ helper trên thiết bị.
  std::string blob, prefs_blob;
  auto collected = guard_status(ErrorCode::kDeviceUnavailable, "không đọc được dữ liệu app", [&] {
    if (offline) {
      blob = platform::read_file(request.from_prefs_file);
      return;
    }
    if (!adb::device_online(device_id)) {
      throw std::runtime_error("thiết bị " + device_id + " không online (adb get-state)");
    }
    turing::ProbeOptions probe;
    probe.target_package = request.package;
    if (!turing::helper_installed(device_id, probe)) {
      throw std::runtime_error(
          "Tencent Helper (" + probe.package +
          ") chưa được cài hoặc quá cũ — cài/cập nhật APK rồi chạy lại");
    }
    helper = turing::collect_helper_snapshot(device_id, probe);
    blob = helper.account_blob;
    prefs_blob = helper.prefs_blob;
    raw = util::Json::parse(helper.raw_json);
  });
  if (!collected) return collected.error();
  outcome.root_granted = helper.root_granted;

  // Helper không có root ⇒ không đọc được vùng riêng của QQLive. Chỉ cập nhật
  // được account đã lưu; account ĐẦU TIÊN bắt buộc phải có credential riêng.
  if (!offline && blob.empty() && prefs_blob.empty()) {
    const std::string existing_id = storage::default_account();
    const std::string existing_path =
        existing_id.empty() ? "" : storage::account_path(existing_id);
    if (existing_path.empty() || !storage::account_exists(existing_path)) {
      return make_error(ErrorCode::kPermissionDenied,
                        "helper chưa được cấp root nên không đọc được credential, "
                        "và chưa có account nào để tái dùng",
                        "cấp quyền root trong helper rồi chạy lại");
    }
    auto existing = guard<account::Account>(ErrorCode::kIo, "không nạp được account cũ",
                                            [&] { return storage::load_account(existing_path); });
    if (!existing) return existing.error();
    account = existing.value();
    reused_existing = true;
  }

  // 3) Nguồn chuẩn: account-state JSON trong MMKV (đủ vusession/vurefresh/WX).
  const bool from_json = !blob.empty() && account::parse_account_json(blob, account);
  if (const std::string guid_value = account::extract_guid(blob); !guid_value.empty()) {
    account.guid = guid_value;
  }

  // 4) Cookie bổ sung usid/qimei36 — chọn đúng bản cookie theo vuid.
  std::string cookie = account::find_login_cookie_for_vuid(blob, account.vuid);
  if (cookie.empty()) cookie = account::find_login_cookie_in_prefs(prefs_blob);
  if (cookie.empty() && offline) cookie = account::find_login_cookie(blob);
  const auto cookie_map = account::parse_cookie(cookie);
  if (!from_json) account::fill_from_cookie(account, cookie_map);

  const auto cookie_value = [&cookie_map](std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
      const auto it = cookie_map.find(key);
      if (it != cookie_map.end() && !it->second.empty()) return it->second;
    }
    return std::string();
  };
  if (account.usid.empty()) account.usid = cookie_value({"usid"});
  if (account.qimei36.empty()) {
    account.qimei36 = cookie_value({"vdevice_qimei36", "qimei36", "qimei"});
  }
  if (account.vuid.empty()) account.vuid = cookie_value({"vuserid", "v_vuserid"});

  if (!from_json && cookie.empty() && !reused_existing) {
    return make_error(ErrorCode::kNotFound,
                      "không thấy account JSON hay cookie đăng nhập",
                      "cần các dấu hiệu vusession= và vdevice_qimei36= — app đã đăng nhập chưa?");
  }
  outcome.source = from_json ? "account-JSON" : (reused_existing ? "existing-account" : "cookie");

  // 5) Thông tin thiết bị lấy từ ảnh chụp thô của helper.
  if (!request.skip_device_info && !device_id.empty()) {
    auto filled = guard_status(ErrorCode::kProtocol, "ảnh chụp thiết bị thiếu dữ liệu", [&] {
      const auto& props = raw.at("props");
      const auto& device = raw.at("device");
      const auto& app_raw = raw.at("app");
      account.model = util::json_string(props, "ro.product.model");
      account.brand = util::json_string(props, "ro.product.brand");
      account.manufacturer = util::json_string(props, "ro.product.manufacturer");
      account.os_version = util::json_string(props, "ro.build.version.release");
      account.android_api_level = util::json_string(props, "ro.build.version.sdk");
      account.app_ver = util::json_string(app_raw, "qqlive_version_name");
      account.app_sub_build = util::json_string(app_raw, "qqlive_version_code");
      account.download_proxy_version = util::json_string(app_raw, "download_proxy_version");
      account.screen_width = util::json_int(device, "screen_width");
      account.screen_height = util::json_int(device, "screen_height");
      account.screen_dpi = util::json_int(device, "screen_dpi");
      account.raw_fingerprint_json = helper.raw_json;
    });
    if (!filled) return filled.error();

    if (helper.root_granted) {
      account.turing_prefs_xml = helper.turing_prefs_blob;
      account.k26_save_status_raw = helper.k26_save_status;
    }
    if (helper.root_granted || !reused_existing) {
      try {
        account.turing = turing::material_from_snapshot(helper, request.package, raw);
      } catch (const std::exception& e) {
        outcome.warnings.push_back(std::string("không lấy được vật liệu Turing: ") + e.what());
      }
    }
  }

  if (account.app_build.empty()) account.app_build = account::derive_build(account.app_ver);
  if (account.platform.empty()) account.platform = "1000005";
  if (account.sdtfrom.empty()) account.sdtfrom = "v5000";

  if (account.qimei36.empty() || account.guid.empty()) {
    if (const Status recovered = recover_identity(account); !recovered) {
      outcome.warnings.push_back("không khôi phục được identity: " +
                                 recovered.error().to_string());
    }
  }

  // 6) Siêu dữ liệu + kiểm tra.
  const int64_t now = util::now_seconds();
  account.source_method = offline ? "from-prefs" : "adb";
  account.source_device_id = device_id;
  account.source_pkg = request.package;
  account.created_at = now;
  account.updated_at = now;

  if (const std::string missing = account::missing_required(account); !missing.empty()) {
    outcome.warnings.push_back("thiếu trường bắt buộc: " + missing + " (vẫn ghi file)");
  }
  if (account.vuid.empty()) {
    return make_error(ErrorCode::kNotFound,
                      "thiếu vuid nên không xác định được tên file kết quả");
  }

  // 7) Ghi (sao lưu bản cũ, giữ created_at gốc, ưu tiên vật liệu đọc bằng root).
  const std::string path =
      request.output_path.empty() ? storage::account_path(account.vuid) : request.output_path;
  turing::Material previous_turing;
  bool has_previous_turing = false;
  if (storage::account_exists(path)) {
    try {
      const account::Account previous = storage::load_account(path);
      if (previous.created_at) account.created_at = previous.created_at;
      previous_turing = previous.turing;
      has_previous_turing = true;
      storage::save_account(storage::backup_path(account.vuid, now), previous);
    } catch (const std::exception& e) {
      outcome.warnings.push_back(std::string("không sao lưu được bản cũ: ") + e.what());
    }
  }
  turing::apply_material_fallback(account.turing,
                                  has_previous_turing ? &previous_turing : nullptr);

  auto saved = guard_status(ErrorCode::kIo, "không ghi được account",
                            [&] { storage::save_account(path, account); });
  if (!saved) return saved.error();
  outcome.path = path;

  const bool is_first = storage::default_account().empty();
  if (request.set_default || is_first) {
    storage::set_default_account(account.vuid);
    outcome.became_default = true;
  }
  return outcome;
}

// ── Thông tin account ────────────────────────────────────────────────────────

Result<AccountInfo> AccountService::info(const std::string& account_id, bool local_only,
                                         bool allow_refresh) const {
  auto loaded = load(account_id);
  if (!loaded) return loaded.error();

  AccountInfo info;
  info.account = loaded.value();
  const std::string path = path_for(account_id);

  // Identity thiếu (bundle cũ / dump không root): bổ sung rồi lưu lại.
  if (info.account.qimei36.empty() || info.account.guid.empty()) {
    if (const Status recovered = recover_identity(info.account); !recovered) {
      return recovered.error();
    }
    info.identity_recovered = true;
    info.account.updated_at = util::now_seconds();
    auto saved = guard_status(ErrorCode::kIo, "không lưu được identity vừa khôi phục",
                              [&] { storage::save_account(path, info.account); });
    if (!saved) return saved.error();
  }

  if (local_only) return info;
  info.queried_server = true;

  if (account::missing_required(info.account).find("vusession") != std::string::npos) {
    return make_error(ErrorCode::kAuthExpired, "thiếu vusession nên không truy vấn được VIP");
  }

  if (allow_refresh) {
    auto refreshed = ensure_fresh_session(info.account, path);
    if (!refreshed) {
      // Không chặn: vẫn thử truy vấn bằng vusession hiện có.
      info.warnings.push_back("tự làm mới phiên thất bại: " + refreshed.error().to_string());
    } else {
      info.refreshed = refreshed.value();
    }
  }

  const int64_t now = util::now_seconds();
  const uint64_t now_ms = static_cast<uint64_t>(now) * 1000ull;

  // ── VIP ────────────────────────────────────────────────────────────────────
  auto vip_query = guard<std::string>(ErrorCode::kNetwork, "truy vấn VIP thất bại", [&] {
    const std::string body = vip::build_vip_request_body(vip::kDefaultVipTypes);
    const OrderedKV extra = {{"vb_control_tag", "160"}};
    const std::string framed =
        trpc::build_qqlive_frame(vip::kService, vip::kMethod, body, info.account.to_session(),
                                 vip::kProtocolId, now_ms, extra);
    std::string head, response_body;
    post_trpc_frame(framed, info.account, options_.timeout_sec, head, response_body);
    if (const std::string error = vip::response_error(head); !error.empty()) {
      throw std::runtime_error(error);
    }
    return vip::summarize(vip::parse_vip_response(response_body), now);
  });
  if (!vip_query) {
    return make_error(vip_query.error().code, vip_query.error().message,
                      vip_query.error().detail +
                          " (vusession có thể đã hết hạn — thử `auth --refresh`)");
  }
  info.vip_summary = vip_query.value();

  // ── Số dư kim cương (User Center) ─────────────────────────────────────────
  // GetVipUserInfo.score là ĐIỂM chứ không phải kim cương, nên phải hỏi đúng
  // endpoint MVL mà trang cá nhân dùng. Lỗi ở đây KHÔNG làm hỏng cả lệnh.
  auto diamonds = guard<std::string>(ErrorCode::kNetwork, "không đọc được số dư kim cương", [&] {
    const std::string body = usercenter::build_page_request(info.account.vuid);
    const OrderedKV extra = {{"can_show_ip_hall_entrance", "1"}};
    const std::string framed = trpc::build_qqlive_frame(
        usercenter::kService, usercenter::kMethod, body, info.account.to_session(),
        usercenter::kProtocolId, now_ms, extra);
    std::string head, response_body;
    post_trpc_frame(framed, info.account, options_.timeout_sec, head, response_body);
    if (const std::string error = vip::response_error(head); !error.empty()) {
      throw std::runtime_error(error);
    }
    return usercenter::extract_diamond_balance(response_body);
  });
  if (diamonds) {
    info.diamond_balance = diamonds.value();
  } else {
    info.warnings.push_back(diamonds.error().to_string());
  }
  return info;
}

}  // namespace tencent::service
