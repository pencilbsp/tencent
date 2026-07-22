// usercenter.h - Minimal User Center MVL request/response support.
#pragma once

#include <string>

namespace tencent::usercenter {

constexpr const char* kService =
    "trpc.multi_vector_layout.mvl_controller.MVLPersonalService";
constexpr const char* kMethod =
    "/trpc.multi_vector_layout.mvl_controller.MVLPersonalService/getMVLPage";
constexpr int kProtocolId = 110;

// MVLPageRequest{page_params}: page_id=default, page_type=personal, and account IDs.
std::string build_page_request(const std::string& vuid);

// Returns UserCenterVIPInfo.diamond_block.sub_title, or an empty string if absent.
std::string extract_diamond_balance(const std::string& response_body);

}  // namespace tencent::usercenter
