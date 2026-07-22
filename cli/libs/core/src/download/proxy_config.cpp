#include "tencent/core/download/proxy_config.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "tencent/core/net/http_client.h"
#include "tencent/core/common/json.h"

namespace tencent::download_proxy {
namespace {

std::string url_encode(const std::string& value) {
  std::ostringstream out;
  out << std::uppercase << std::hex;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '.' || c == '~') {
      out << static_cast<char>(c);
    } else {
      out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
  }
  return out.str();
}

int json_int(const nlohmann::json& object, const char* key, int fallback,
             bool& present) {
  auto it = object.find(key);
  if (it == object.end()) return fallback;
  present = true;
  if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
  if (it->is_number_integer()) return it->get<int>();
  if (it->is_string()) {
    try {
      return std::stoi(it->get<std::string>());
    } catch (const std::exception&) {
      return fallback;
    }
  }
  return fallback;
}

}  // namespace

std::string build_config_url(const Context& context) {
  if (context.version.empty()) throw std::invalid_argument("Missing DownloadProxy version");
  if (context.platform.empty()) throw std::invalid_argument("Missing platform");
  if (context.guid.empty()) throw std::invalid_argument("Missing guid");
  if (context.qimei36.empty()) throw std::invalid_argument("Missing qimei36");
  if (context.device_model.empty()) throw std::invalid_argument("Missing device model");

  return "https://appcfg.v.qq.com/getconf?cmd=data_transport&name=tp_proxy"
         "&subver=V_" +
         url_encode(context.version) + "&platform=" + url_encode(context.platform) +
         "&guid=" + url_encode(context.guid) + "&qimei36=" +
         url_encode(context.qimei36) + "&dev=" + url_encode(context.device_model);
}

Config parse_config_response(const std::string& response) {
  Config config;
  config.raw_response = response;
  nlohmann::json envelope = nlohmann::json::parse(response);
  if (envelope.value("code", -1) != 0)
    throw std::runtime_error("appcfg code=" +
                             std::to_string(envelope.value("code", -1)) +
                             " msg=" + envelope.value("msg", std::string()));

  nlohmann::json data;
  if (envelope.contains("data") && envelope["data"].is_string()) {
    const std::string text = envelope["data"].get<std::string>();
    if (!text.empty()) data = nlohmann::json::parse(text);
  } else if (envelope.contains("data") && envelope["data"].is_object()) {
    data = envelope["data"];
  }
  if (!data.is_object()) return config;

  config.cdn_strategy =
      json_int(data, "CDNStrategy", config.cdn_strategy, config.has_cdn_strategy);
  config.quick_download_cdn_strategy =
      json_int(data, "QuickDownloadCDNStrategy",
               config.quick_download_cdn_strategy,
               config.has_quick_download_cdn_strategy);
  return config;
}

Config fetch_config(const Context& context, long timeout_sec, bool insecure,
                    const std::string& proxy_url) {
  const std::string url = build_config_url(context);
  auto [status, response] =
      http::http_get(url, {}, timeout_sec, insecure, proxy_url);
  if (status != 200)
    throw std::runtime_error("appcfg HTTP " + std::to_string(status));
  return parse_config_response(http::maybe_gunzip(response));
}

}  // namespace tencent::download_proxy
