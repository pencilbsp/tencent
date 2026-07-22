// paths.cpp — Xem include/tencent/core/storage/paths.h.
//
// Bố cục thư mục dữ liệu ~/.tencent. Đường dẫn HOME lấy qua lớp platform nên
// lõi không tự đọc biến môi trường.
#include "tencent/core/storage/paths.h"

#include <filesystem>

#include "tencent/core/common/json.h"
#include "tencent/core/model/account.h"
#include "tencent/platform/filesystem.h"
#include "tencent/platform/paths.h"

namespace tencent::storage {
namespace fs = std::filesystem;
namespace {

// Gốc dữ liệu có thể bị ghi đè lúc chạy (test/GUI). Rỗng = dùng mặc định của
// nền tảng.
std::string& home_override() {
  static std::string value;
  return value;
}

}  // namespace

std::string app_home() {
  if (!home_override().empty()) return home_override();
  return platform::default_app_data_directory();
}

void set_app_home(const std::string& path) { home_override() = path; }

std::string accounts_dir() { return app_home() + "/accounts"; }
std::string config_path() { return app_home() + "/config.json"; }
std::string account_path(const std::string& vuid) { return accounts_dir() + "/" + vuid; }

std::string legacy_account_path(const std::string& vuid) {
  return accounts_dir() + "/" + vuid + ".json";
}

std::string backup_path(const std::string& vuid, int64_t timestamp) {
  return app_home() + "/backups/" + vuid + "/" + std::to_string(timestamp);
}

std::string videos_dir() { return app_home() + "/videos"; }

void set_owner_only_file(const std::string& path) {
  std::error_code ec;
  fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write,
                  fs::perm_options::replace, ec);
}

void set_owner_only_directory(const std::string& path) {
  std::error_code ec;
  fs::permissions(path,
                  fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec,
                  fs::perm_options::replace, ec);
}

void ensure_dirs() {
  platform::ensure_directory(accounts_dir());
  set_owner_only_directory(app_home());
  set_owner_only_directory(accounts_dir());
}

std::string default_account() {
  try {
    const std::string text = platform::read_file(config_path());
    const auto config = util::OrderedJson::parse(text);
    return util::json_string(config, "default_account");
  } catch (const std::exception&) {
    return "";   // chưa có config.json hoặc file hỏng → coi như chưa đặt
  }
}

void set_default_account(const std::string& vuid) {
  util::OrderedJson config = util::OrderedJson::object();
  try {
    config = util::OrderedJson::parse(platform::read_file(config_path()));
    if (!config.is_object()) config = util::OrderedJson::object();
  } catch (const std::exception&) {
    // Không có file cũ (hoặc hỏng) → bắt đầu từ object rỗng.
  }
  config["schema_version"] = account::kSchemaVersion;
  config["default_account"] = vuid;

  platform::ensure_directory(app_home());
  set_owner_only_directory(app_home());
  platform::write_file_atomic(config_path(), config.dump(2) + "\n");
  set_owner_only_file(config_path());
}

}  // namespace tencent::storage
