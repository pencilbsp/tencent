// paths.h — Các đường dẫn phụ thuộc hệ điều hành.
//
// Lõi nghiệp vụ KHÔNG được tự đọc biến môi trường HOME: mọi đường dẫn gốc đi
// qua đây, nhờ vậy test có thể chuyển hướng bằng cách đặt lại thư mục gốc và
// bản GUI sau này có thể dùng thư mục dữ liệu riêng của nền tảng.
#pragma once

#include <string>

namespace tencent::platform {

// Thư mục home của người dùng hiện tại.
// POSIX: $HOME, fallback getpwuid(). Windows: %USERPROFILE%.
// Trả "" nếu không xác định được.
std::string home_directory();

// Thư mục dữ liệu của ứng dụng: <home>/.tencent
// Đây là gốc của accounts/, backups/, videos/ (xem storage/paths.h của core).
std::string default_app_data_directory();

}  // namespace tencent::platform
