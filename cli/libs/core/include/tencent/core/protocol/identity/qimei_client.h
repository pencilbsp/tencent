// qimei_client.h - Qimei REGISTER (/ola/v2) identity bootstrap.
#pragma once

#include <cstdint>
#include <string>

namespace tencent::qimei {

struct DeviceProfile {
  std::string app_version;
  std::string android_version;
  std::string android_api_level;
  std::string model;
  std::string manufacturer;
  std::string brand;
  std::string board;
  std::string device;
  std::string product_name;
  std::string build_host;
  std::string first_api_level;
  std::string android_id;
  std::string kernel;
  std::string boot_id;
  std::string device_signal_kv;
  int timezone_offset_minutes = 0;
  int64_t current_time_ms = 0;
  int64_t elapsed_realtime_ms = 0;
  int64_t first_install_ms = 0;
};

struct Result {
  std::string qimei16;
  std::string qimei36;
};

// Maps the helper's exact turing-raw/1 snapshot to a fresh REGISTER profile.
DeviceProfile profile_from_raw_json(const std::string& raw_json);

// Pure builders exposed for deterministic tests.
std::string build_tail17(const DeviceProfile& profile);
std::string build_register_plaintext(const DeviceProfile& profile,
                                     const std::string& request_id,
                                     int64_t now_sec);
std::string make_sn(const std::string& preimage, uint32_t phase1_state_w0);

// Sends a cpt=1 REGISTER with empty cached qimei fields and decrypts fields 1/2.
Result register_device(const DeviceProfile& profile, long timeout_sec = 20);

}  // namespace tencent::qimei
