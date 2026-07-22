// strings.h — Thao tác chuỗi dùng chung.
//
// Gom các helper trước đây bị lặp ở account_file.cpp, adb.cpp, proxy.cpp và
// tencent_cli_main.cpp (mỗi nơi một bản `trim` riêng). Chỉ để những gì thực sự
// dùng ≥2 nơi — không biến file này thành bãi rác tiện ích.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tencent::util {

// Cắt khoảng trắng hai đầu (space, tab, CR, LF).
std::string trim(const std::string& text);

bool starts_with(const std::string& text, const std::string& prefix);
bool ends_with(const std::string& text, const std::string& suffix);

// Tách theo ký tự phân cách. keep_empty=false thì bỏ các phần rỗng.
std::vector<std::string> split(const std::string& text, char delimiter,
                               bool keep_empty = true);

std::string join(const std::vector<std::string>& parts, const std::string& separator);

std::string to_lower(const std::string& text);

// Thay thế TẤT CẢ lần xuất hiện của `from` bằng `to`.
std::string replace_all(std::string text, const std::string& from, const std::string& to);

// Định dạng dung lượng cho người đọc: 1.4 GB, 927.3 MB, 512 B…
std::string human_bytes(uint64_t bytes);

// Chuyển an toàn sang số; trả `fallback` nếu chuỗi rỗng/không phải số.
int64_t to_int64(const std::string& text, int64_t fallback = 0);

}  // namespace tencent::util
