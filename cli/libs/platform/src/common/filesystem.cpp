// filesystem.cpp — Xem include/tencent/platform/filesystem.h.
//
// Dùng chung cho mọi nền tảng: <filesystem> của C++17 đã che khác biệt đường
// dẫn/phân cách. Không có #ifdef nào trong file này.
#include "tencent/platform/filesystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

namespace tencent::platform {
namespace fs = std::filesystem;

std::string read_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Không đọc được file: " + path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void write_file(const std::string& path, const std::string& data) {
  const fs::path target(path);
  if (target.has_parent_path()) {
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("Không ghi được file: " + path);
  output.write(data.data(), static_cast<std::streamsize>(data.size()));
  if (!output) throw std::runtime_error("Ghi file thất bại: " + path);
}

void write_file_atomic(const std::string& path, const std::string& data) {
  const fs::path target(path);
  // Tên tạm ngẫu nhiên trong CÙNG thư mục để rename là thao tác nguyên tử
  // (rename qua khác filesystem không đảm bảo tính nguyên tử).
  static std::mt19937_64 rng{std::random_device{}()};
  const fs::path temp =
      target.parent_path() /
      (target.filename().string() + ".tmp" + std::to_string(rng() & 0xFFFFFFFFu));

  write_file(temp.string(), data);
  std::error_code ec;
  fs::rename(temp, target, ec);
  if (ec) {
    // Một số hệ thống không cho rename đè; thử xoá đích rồi rename lại.
    fs::remove(target, ec);
    fs::rename(temp, target, ec);
  }
  if (ec) {
    fs::remove(temp, ec);
    throw std::runtime_error("Không thay thế được file: " + path);
  }
}

bool file_exists(const std::string& path) {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

bool directory_exists(const std::string& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

void ensure_directory(const std::string& path) {
  std::error_code ec;
  fs::create_directories(path, ec);
  if (ec && !fs::is_directory(path, ec)) {
    throw std::runtime_error("Không tạo được thư mục: " + path);
  }
}

uint64_t directory_size_bytes(const std::string& path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) return 0;
  uint64_t total = 0;
  for (fs::recursive_directory_iterator it(path, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    std::error_code entry_ec;
    if (it->is_regular_file(entry_ec)) {
      const auto size = it->file_size(entry_ec);
      if (!entry_ec) total += size;
    }
  }
  return total;
}

std::vector<std::string> list_directory(const std::string& path) {
  std::vector<std::string> names;
  std::error_code ec;
  for (fs::directory_iterator it(path, ec), end; it != end; it.increment(ec)) {
    if (ec) break;
    names.push_back(it->path().filename().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

uint64_t remove_all(const std::string& path) {
  std::error_code ec;
  const auto removed = fs::remove_all(path, ec);
  if (ec) return 0;
  return static_cast<uint64_t>(removed);
}

std::string join_path(const std::string& base, const std::string& child) {
  return (fs::path(base) / child).string();
}

}  // namespace tencent::platform
