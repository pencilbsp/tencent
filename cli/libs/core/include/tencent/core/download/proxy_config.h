#pragma once

#include <string>

namespace tencent::download_proxy {

struct Context {
  std::string version;
  std::string platform;
  std::string guid;
  std::string qimei36;
  std::string device_model;
};

struct Config {
  int cdn_strategy = 0;
  int quick_download_cdn_strategy = 0;
  bool has_cdn_strategy = false;
  bool has_quick_download_cdn_strategy = false;
  std::string raw_response;
};

std::string build_config_url(const Context& context);
Config parse_config_response(const std::string& response);
Config fetch_config(const Context& context, long timeout_sec = 20,
                    bool insecure = false,
                    const std::string& proxy_url = "");

}  // namespace tencent::download_proxy
