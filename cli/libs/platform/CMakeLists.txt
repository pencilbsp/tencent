# libs/platform — tencent::platform
#
# Tầng THẤP NHẤT: mọi lời gọi hệ điều hành (tiến trình con, đường dẫn người
# dùng, socket thô) đều nằm ở đây. Lõi nghiệp vụ không được include <unistd.h>,
# <sys/socket.h> hay gọi popen/getenv trực tiếp.
#
# Nguồn được CHỌN theo nền tảng: không có #ifdef trong file .cpp (trừ khác biệt
# rất nhỏ như MSG_NOSIGNAL) — mỗi hệ điều hành một thư mục riêng.

set(TENCENT_PLATFORM_SOURCES
  src/common/filesystem.cpp)    # <filesystem> C++17: dùng chung mọi nền tảng

if(WIN32)
  list(APPEND TENCENT_PLATFORM_SOURCES src/windows/platform_windows.cpp)
else()
  list(APPEND TENCENT_PLATFORM_SOURCES
    src/posix/process_posix.cpp
    src/posix/paths_posix.cpp
    src/posix/socket_posix.cpp)
endif()

add_library(tencent_platform STATIC ${TENCENT_PLATFORM_SOURCES})
add_library(tencent::platform ALIAS tencent_platform)

target_include_directories(tencent_platform PUBLIC include)
target_link_libraries(tencent_platform
  PRIVATE tencent::project_options
  PUBLIC Threads::Threads)
