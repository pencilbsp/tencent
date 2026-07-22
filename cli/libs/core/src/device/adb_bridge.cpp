// adb_bridge.cpp — Xem include/tencent/core/device/adb_bridge.h.
#include "tencent/core/device/adb_bridge.h"

#include <regex>
#include <sstream>
#include <stdexcept>

#include "tencent/core/common/strings.h"
#include "tencent/platform/process.h"

namespace tencent::adb {
namespace {

constexpr const char* kAdb = "adb";

// Chạy adb với đối số RỜI. Không qua shell của máy chủ ⇒ không phải quote gì ở
// phía host; `2>/dev/null` ngày xưa nay là "bỏ qua ProcessResult::stderr_data".
std::string run_adb(const std::vector<std::string>& args) {
  const platform::ProcessResult result = platform::run_process(kAdb, args);
  if (!result.spawned) {
    throw std::runtime_error("không chạy được `adb` (đã cài và có trong PATH chưa?)");
  }
  return result.stdout_data;
}

}  // namespace

std::vector<std::string> list_devices() {
  std::vector<std::string> devices;
  const platform::ProcessResult result = platform::run_process(kAdb, {"devices"});
  if (!result.spawned) return devices;   // chưa cài adb → coi như không có máy

  std::istringstream stream(result.stdout_data);
  std::string line;
  bool first = true;
  while (std::getline(stream, line)) {
    if (first) {                         // dòng "List of devices attached"
      first = false;
      continue;
    }
    const std::string trimmed = util::trim(line);
    if (trimmed.empty()) continue;
    const std::size_t separator = trimmed.find_first_of(" \t");
    if (separator == std::string::npos) continue;
    // Chỉ nhận máy ở trạng thái "device"; bỏ offline/unauthorized.
    if (util::trim(trimmed.substr(separator)) == "device") {
      devices.push_back(trimmed.substr(0, separator));
    }
  }
  return devices;
}

std::string run(const std::string& device_id, const std::vector<std::string>& args) {
  std::vector<std::string> full = {"-s", device_id};
  full.insert(full.end(), args.begin(), args.end());
  return run_adb(full);
}

std::string device_quote(const std::string& value) {
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') quoted += "'\\''";   // đóng nháy, chèn ' đã thoát, mở lại
    else quoted.push_back(c);
  }
  quoted.push_back('\'');
  return quoted;
}

std::string shell(const std::string& device_id, const std::string& command) {
  return run(device_id, {"shell", command});
}

std::string su_shell(const std::string& device_id, const std::string& command) {
  return run(device_id, {"shell", "su -c " + device_quote(command)});
}

std::string exec_out(const std::string& device_id, const std::string& command) {
  return run(device_id, {"exec-out", command});
}

std::string su_cat(const std::string& device_id, const std::string& glob_path) {
  // glob KHÔNG được quote để shell của thiết bị tự khai triển `*`.
  return exec_out(device_id, "su -c " + device_quote("cat " + glob_path + " 2>/dev/null"));
}

std::string getprop(const std::string& device_id, const std::string& key) {
  return util::trim(shell(device_id, "getprop " + device_quote(key)));
}

AppVersion app_version(const std::string& device_id, const std::string& package) {
  const std::string dump = shell(device_id, "dumpsys package " + device_quote(package));
  AppVersion version;
  std::smatch match;
  const std::regex name_pattern("versionName=([^\\s]+)");
  if (std::regex_search(dump, match, name_pattern)) version.version_name = match[1].str();
  const std::regex code_pattern("versionCode=([0-9]+)");
  if (std::regex_search(dump, match, code_pattern)) version.version_code = match[1].str();
  return version;
}

int64_t package_uid(const std::string& device_id, const std::string& package) {
  std::smatch match;
  std::string out = shell(device_id, "cmd package list packages -U " + device_quote(package));
  const std::regex uid_pattern("uid:([0-9]+)");
  if (std::regex_search(out, match, uid_pattern)) return util::to_int64(match[1].str());

  // Máy cũ không có `cmd package -U`: đọc từ dumpsys.
  out = shell(device_id, "dumpsys package " + device_quote(package));
  const std::regex fallback_pattern("(?:userId|appId)=([0-9]+)");
  if (std::regex_search(out, match, fallback_pattern)) return util::to_int64(match[1].str());
  return 0;
}

void screen_size(const std::string& device_id, int64_t& width, int64_t& height) {
  width = height = 0;
  const std::string out = shell(device_id, "wm size");
  std::smatch match;
  // Ưu tiên "Physical size"; "Override size" xuất hiện khi người dùng đổi độ
  // phân giải hiển thị.
  const std::regex pattern("(?:Physical|Override) size:\\s*([0-9]+)x([0-9]+)");
  if (std::regex_search(out, match, pattern)) {
    width = util::to_int64(match[1].str());
    height = util::to_int64(match[2].str());
  }
}

int64_t screen_density(const std::string& device_id) {
  const std::string out = shell(device_id, "wm density");
  std::smatch match;
  const std::regex pattern("(?:Physical|Override) density:\\s*([0-9]+)");
  if (std::regex_search(out, match, pattern)) return util::to_int64(match[1].str());
  return 0;
}

bool device_online(const std::string& device_id) {
  if (!platform::program_exists(kAdb)) return false;
  try {
    return util::trim(run(device_id, {"get-state"})) == "device";
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace tencent::adb
