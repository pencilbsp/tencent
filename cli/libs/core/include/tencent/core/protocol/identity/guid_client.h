// guid_client.h - QQLive StGuidGetRequest (UniCmd/JCE cmd 57728).
#pragma once

#include <cstdint>
#include <string>

namespace tencent::guid {

struct Context {
  std::string qimei36;
  std::string usid;
  std::string omg_id;
  std::string version_name;
  std::string version_code;
  std::string channel_id = "20563";
  std::string model;
  std::string manufacturer;
  std::string os_version;
  int64_t screen_width = 0;
  int64_t screen_height = 0;
  int64_t screen_dpi = 0;
};

struct Result {
  int err_code = -1;
  std::string err_message;
  std::string guid;
  int64_t server_time = 0;
};

std::string build_recovery_frame(const Context& context, uint64_t request_id,
                                 int64_t now_sec);
Result parse_response(const std::string& raw);
Result recover(const Context& context, long timeout_sec = 20);

}  // namespace tencent::guid
