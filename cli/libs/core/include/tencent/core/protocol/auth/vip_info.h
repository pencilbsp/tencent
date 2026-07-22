// vipinfo.h — QueryVipInfo / GetVipUserInfo: dựng request body + parse response.
//
// Port ../analytics/app/vipinfo_client.py:build_vip_request_body + ../analytics/app/vipinfo_response.py
// (parse_vip_*, summarize) + ../analytics/app/vipinfo_client.py:_response_error.
// Tài liệu: ../analytics/app/userinfo_api.md §2.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tencent::vip {

// Loại VIP mặc định query (../analytics/app/vipinfo_client.py:40): video SVIP + sport/skp/
// family/kids.
inline const std::vector<int> kDefaultVipTypes = {3, 3006, 3007, 3013, 3012};

constexpr const char* kService = "trpc.query_vipinfo.vipinfo.QueryVipInfo";
constexpr const char* kMethod = "/trpc.query_vipinfo.vipinfo.QueryVipInfo/GetVipUserInfo";
constexpr int kProtocolId = 110;

// VipBaseInfo (../analytics/app/vipinfo_response.py:41-48).
struct VipBase {
  bool is_vip = false;
  int64_t begin_time = 0;
  int64_t end_time = 0;
  int level = 0;
};

// VipExtraInfo rút gọn (../analytics/app/vipinfo_response.py:51-63).
struct VipExtra {
  std::string end_msg;
  std::string icon_url;  // chỉ khi f3 là URL bắt đầu "http"
};

// VipUserInfo (../analytics/app/vipinfo_response.py:66-75).
struct VipUserInfo {
  bool has_base = false;
  VipBase base;
  bool has_extra = false;
  VipExtra extra;
};

// VipUserInfoResponse đã decode (../analytics/app/vipinfo_response.py:78-104).
struct VipResponse {
  bool has_hollywood = false;
  VipUserInfo hollywood;
  bool has_nba = false;
  VipUserInfo nba;
  bool has_vvip = false;
  VipUserInfo vvip;
  bool has_json = false;
  std::string json_data;            // raw string (summarize không dùng)
  std::map<int, VipUserInfo> by_type;  // map<int32,VipUserInfo>, sort theo key
};

// ../analytics/app/vipinfo_client.py:53-58 — field 2 repeated varint.
std::string build_vip_request_body(const std::vector<int>& vip_types);

// ../analytics/app/vipinfo_response.py:78-104.
VipResponse parse_vip_response(const std::string& body);

// ../analytics/app/vipinfo_response.py:113-133 — now_ts inject (Python dùng datetime.now()).
std::string summarize(const VipResponse& resp, int64_t now_ts);

// ../analytics/app/vipinfo_client.py:85-95 — TRPC head f5=ret f6=msg; "" nếu không lỗi.
std::string response_error(const std::string& head);

}  // namespace tencent::vip
