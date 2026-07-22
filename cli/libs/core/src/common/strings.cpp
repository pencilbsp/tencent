// strings.cpp — Xem include/tencent/core/common/strings.h.
#include "tencent/core/common/strings.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace tencent::util {

std::string trim(const std::string& text) {
  const char* kSpace = " \t\r\n";
  const std::size_t first = text.find_first_not_of(kSpace);
  if (first == std::string::npos) return "";
  const std::size_t last = text.find_last_not_of(kSpace);
  return text.substr(first, last - first + 1);
}

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> split(const std::string& text, char delimiter, bool keep_empty) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find(delimiter, start);
    if (end == std::string::npos) end = text.size();
    std::string part = text.substr(start, end - start);
    if (keep_empty || !part.empty()) parts.push_back(std::move(part));
    if (end == text.size()) break;
    start = end + 1;
  }
  return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& separator) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) out += separator;
    out += parts[i];
  }
  return out;
}

std::string to_lower(const std::string& text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  if (from.empty()) return text;
  std::size_t at = 0;
  while ((at = text.find(from, at)) != std::string::npos) {
    text.replace(at, from.size(), to);
    at += to.size();   // không quét lại phần vừa chèn (tránh vòng lặp vô hạn)
  }
  return text;
}

std::string human_bytes(uint64_t bytes) {
  // Dùng bội số 1024 (KiB/MiB) nhưng hiển thị nhãn quen thuộc KB/MB/GB.
  static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};
  double value = static_cast<double>(bytes);
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  char buffer[64];
  if (unit == 0) {
    std::snprintf(buffer, sizeof(buffer), "%llu B",
                  static_cast<unsigned long long>(bytes));
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, kUnits[unit]);
  }
  return buffer;
}

int64_t to_int64(const std::string& text, int64_t fallback) {
  const std::string clean = trim(text);
  if (clean.empty()) return fallback;
  try {
    std::size_t consumed = 0;
    const long long parsed = std::stoll(clean, &consumed);
    if (consumed == 0) return fallback;
    return parsed;
  } catch (const std::exception&) {
    return fallback;
  }
}

}  // namespace tencent::util
