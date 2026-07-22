// filesystem.h — Đọc/ghi file và thao tác thư mục.
//
// Toàn bộ hiện thực dựa trên <filesystem> của C++17 nên dùng chung cho mọi nền
// tảng (src/common/filesystem.cpp). Lý do vẫn đặt trong lớp platform: đây là
// điểm duy nhất chạm tới đĩa, giúp mock/redirect dễ và giữ core thuần logic.
//
// Quy ước lỗi: các hàm đọc/ghi ném std::runtime_error kèm đường dẫn khi thất
// bại; các hàm truy vấn (exists/size) không ném.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tencent::platform {

// Đọc trọn file ở chế độ nhị phân (giữ nguyên byte 0x00 và CRLF).
std::string read_file(const std::string& path);

// Ghi đè file ở chế độ nhị phân; tự tạo thư mục cha nếu chưa có.
void write_file(const std::string& path, const std::string& data);

// Ghi an toàn: ghi ra file tạm cùng thư mục rồi rename đè lên đích. Dùng cho
// các file trạng thái (auth.json…) để tiến trình bị ngắt giữa chừng không làm
// hỏng bản cũ.
void write_file_atomic(const std::string& path, const std::string& data);

bool file_exists(const std::string& path);
bool directory_exists(const std::string& path);

// mkdir -p. Không lỗi nếu đã tồn tại.
void ensure_directory(const std::string& path);

// Tổng dung lượng các file thường bên trong thư mục (đệ quy). 0 nếu không có.
uint64_t directory_size_bytes(const std::string& path);

// Danh sách entry con (tên, không phải đường dẫn đầy đủ), đã sắp xếp.
std::vector<std::string> list_directory(const std::string& path);

// Xoá đệ quy. Trả số entry đã xoá; 0 nếu đường dẫn không tồn tại.
uint64_t remove_all(const std::string& path);

// Nối các thành phần đường dẫn theo dấu phân cách của nền tảng.
std::string join_path(const std::string& base, const std::string& child);

}  // namespace tencent::platform
