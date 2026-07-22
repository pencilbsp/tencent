# ProjectOptions.cmake — Thiết lập biên dịch dùng chung cho MỌI target.
#
# Tách khỏi CMakeLists.txt gốc để file gốc chỉ còn nhiệm vụ "lắp ráp" các
# thành phần (xem docs/ARCHITECTURE.md §Build). Không target nào được tự đặt
# chuẩn C++/cảnh báo riêng — luôn link vào `tencent_project_options`.

include_guard(GLOBAL)

# ── Chuẩn ngôn ngữ ───────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # dùng -std=c++17, KHÔNG phải -std=gnu++17

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Xuất compile_commands.json cho clangd/IDE.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Interface target mang cờ cảnh báo ────────────────────────────────────────
# Mọi thư viện/executable của dự án PHẢI link target này (PRIVATE) để có cùng
# một bộ cảnh báo. third_party/ KHÔNG link (được include dạng SYSTEM).
add_library(tencent_project_options INTERFACE)
add_library(tencent::project_options ALIAS tencent_project_options)

if(MSVC)
  target_compile_options(tencent_project_options INTERFACE /W4)
else()
  target_compile_options(tencent_project_options INTERFACE -Wall -Wextra)
endif()
