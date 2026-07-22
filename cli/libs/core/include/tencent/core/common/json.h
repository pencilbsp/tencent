// json.h — Lớp mỏng trên nlohmann/json.
//
// Hai mục đích:
//  1. Chỉ MỘT nơi include thư viện ngoài, kèm bí danh kiểu — sau này đổi thư
//     viện chỉ phải sửa file này.
//  2. Các hàm đọc field "khoan dung": file trạng thái do nhiều phiên bản ghi ra
//     nên thiếu key hoặc sai kiểu là chuyện thường; đọc bằng at() sẽ ném và làm
//     hỏng cả lệnh. Bản cũ lặp lại cặp json_int/json_string ở 3 file.
//
// LƯU Ý: dùng `ordered_json` cho MỌI file/khối cần GIỮ THỨ TỰ key (template,
// query_params, headers). `json` thường sắp xếp key theo alphabet.
#pragma once

#include <string>

#include "nlohmann/json.hpp"

namespace tencent::util {

using Json = nlohmann::json;
using OrderedJson = nlohmann::ordered_json;

// Đọc số nguyên: chấp nhận cả kiểu số lẫn chuỗi số. Thiếu/sai → fallback.
int64_t json_int(const Json& object, const char* key, int64_t fallback = 0);
int64_t json_int(const OrderedJson& object, const char* key, int64_t fallback = 0);

// Đọc chuỗi: số cũng được ép về chuỗi. Thiếu/null → fallback.
std::string json_string(const Json& object, const char* key,
                        const std::string& fallback = "");
std::string json_string(const OrderedJson& object, const char* key,
                        const std::string& fallback = "");

bool json_bool(const Json& object, const char* key, bool fallback = false);

}  // namespace tencent::util
