// account_service.h — Nghiệp vụ TÀI KHOẢN: liệt kê, lấy dữ liệu từ thiết bị,
// làm mới phiên, xem thông tin VIP.
//
// Đây là mặt tiền mà giao diện gọi. Nó KHÔNG in gì ra màn hình và không tự
// quyết định mã thoát — chỉ trả dữ liệu hoặc Error. Toàn bộ phần trình bày
// (bảng biểu, màu sắc, chuỗi ngôn ngữ) thuộc về giao diện.
//
// Luồng nghiệp vụ được gói ở đây (trước đây nằm rải trong tencent_cli_main.cpp):
//   • chọn account mặc định khi người dùng không chỉ định;
//   • khôi phục identity thiếu (qimei36 → guid) theo đúng thứ tự phụ thuộc;
//   • bộ điều hợp vusession: còn hạn thì dùng luôn, hết hạn thì tự làm mới và
//     ghi lại xuống đĩa (kèm sao lưu bản cũ).
#pragma once

#include <string>
#include <vector>

#include "tencent/core/common/result.h"
#include "tencent/core/model/account.h"
#include "tencent/core/model/turing_material.h"

namespace tencent::service {

// ── Dữ liệu trả về ───────────────────────────────────────────────────────────

// Một dòng trong danh sách account (không chứa bí mật).
struct AccountSummary {
  std::string vuid;
  std::string nickname;
  std::string model;
  std::string app_ver;
  std::string qimei36;
  std::string path;
  bool is_default = false;
  bool readable = true;   // false = có file nhưng không đọc/parse được
};

struct RefreshOutcome {
  bool performed = false;          // false = vusession còn hạn, không gọi mạng
  int64_t vusession_expire_ts = 0;
  std::string account_path;
  std::string framed_hex;          // chỉ điền khi dry_run
};

// Yêu cầu lấy dữ liệu từ thiết bị (`auth --adb` / `--from-prefs`).
struct DumpRequest {
  bool use_device = true;                  // false ⇒ bắt buộc có from_prefs_file
  std::string device_id;                   // rỗng = tự chọn nếu chỉ có 1 máy
  std::string package = account::kDefaultPkg;
  std::string from_prefs_file;             // khác rỗng ⇒ chạy offline
  bool skip_device_info = false;           // bỏ qua phần thông tin thiết bị
  bool set_default = false;
  std::string output_path;                 // rỗng = accounts/<vuid>
};

struct DumpOutcome {
  account::Account account;
  std::string path;
  std::string source;            // "account-JSON" | "cookie" | "existing-account"
  bool root_granted = false;
  bool became_default = false;
  bool device_auto_selected = false;
  std::string selected_device_id;
  std::vector<std::string> warnings;   // cảnh báo không chặn (thiếu field…)
};

// Kết quả `auth --info`.
struct AccountInfo {
  account::Account account;
  bool refreshed = false;         // vusession vừa được làm mới trong lần gọi này
  bool identity_recovered = false;
  bool queried_server = false;    // false khi chỉ xem cục bộ
  std::string vip_summary;        // văn bản tóm tắt VIP (đã có sẵn tiếng Việt)
  std::string diamond_balance;    // "" nếu không đọc được
  std::vector<std::string> warnings;
};

// Tuỳ chọn chung của dịch vụ. Đặt ở phạm vi namespace (không lồng trong lớp)
// để dùng được làm giá trị mặc định của hàm dựng.
struct AccountServiceOptions {
  long timeout_sec = 20;
  // Làm mới vusession sớm hơn thời điểm hết hạn bấy nhiêu giây, tránh trường
  // hợp phiên chết ngay giữa một request dài.
  int refresh_skew_sec = 300;
};

// ── Dịch vụ ──────────────────────────────────────────────────────────────────
class AccountService {
 public:
  using Options = AccountServiceOptions;

  explicit AccountService(AccountServiceOptions options = AccountServiceOptions())
      : options_(options) {}

  // Liệt kê account trong thư mục dữ liệu (cả bundle v2 lẫn file v1).
  Result<std::vector<AccountSummary>> list() const;

  // Trả `requested` nếu khác rỗng, ngược lại lấy account mặc định.
  // Lỗi kNotFound nếu không có account nào.
  Result<std::string> resolve_account_id(const std::string& requested) const;

  // Nạp account theo id (đã tự phân giải qua resolve_account_id).
  Result<account::Account> load(const std::string& account_id) const;

  // Đường dẫn thư mục account.
  std::string path_for(const std::string& account_id) const;

  // Thư mục chứa mọi account (chỉ để hiển thị cho người dùng).
  std::string accounts_directory() const;

  // Bổ sung identity còn thiếu theo đúng thứ tự phụ thuộc:
  //   fingerprint thô → đăng ký Qimei (/ola/v2) → xin GUID (StGuidGetRequest).
  // Không ghi đĩa; người gọi tự lưu nếu muốn.
  Status recover_identity(account::Account& account) const;

  // Làm mới vusession và GHI LẠI xuống đĩa (sao lưu bản cũ trước khi đè).
  // dry_run = chỉ dựng khung request rồi trả hex, không gửi đi.
  Result<RefreshOutcome> refresh(const std::string& account_id, bool dry_run) const;

  // Bộ điều hợp dùng trước mỗi lời gọi cần đăng nhập: bảo đảm vusession còn
  // hạn (tự làm mới nếu cần). `account` được cập nhật tại chỗ.
  Result<bool /*đã làm mới*/> ensure_fresh_session(account::Account& account,
                                                   const std::string& account_path) const;

  // Lấy identity + credential từ thiết bị (qua Tencent Helper) hoặc từ file dump.
  Result<DumpOutcome> dump(const DumpRequest& request) const;

  // Thông tin account: phần cục bộ + (nếu không local_only) truy vấn VIP và
  // số dư kim cương từ máy chủ.
  Result<AccountInfo> info(const std::string& account_id, bool local_only,
                           bool allow_refresh) const;

 private:
  AccountServiceOptions options_;
};

}  // namespace tencent::service
