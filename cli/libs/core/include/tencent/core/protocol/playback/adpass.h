// adpass.h — KMM/QAD adpass envelope builder.
//
// Mirrors com.tencent.qadsdk.core.utils.j1.g(...): compact JSON in provider
// insertion order, then Java-style URLEncoder once. The outer getvinfo body
// encoder applies the second URL-encoding layer.
#pragma once

#include <string>

namespace tencent::playback {

struct QadAdpassOptions {
  std::string ad_cookie;
  std::string taid_ticket;
  std::string os_version = "Android_14";
  int wx_version_code = 671106370;
};

std::string java_urlencode(const std::string& s);
std::string build_qad_adpass_json(const QadAdpassOptions& opt);
std::string build_qad_adpass(const QadAdpassOptions& opt);

}  // namespace tencent::playback
