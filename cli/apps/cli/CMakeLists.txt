# apps/cli — executable `tencent-cli`
#
# Lớp mỏng nhất trong dự án: phân tích dòng lệnh (CLI11), gọi tencent::core,
# in kết quả. Không chứa thuật toán, không tự gọi mạng.
# Mỗi lệnh con một file trong src/commands/ để thêm lệnh mới không đụng file khác.

add_executable(tencent-cli
  src/main.cpp
  src/console.cpp
  src/commands/auth_command.cpp
  src/commands/list_command.cpp
  src/commands/getvinfo_command.cpp
  src/commands/download_command.cpp
  src/commands/probe_command.cpp
  src/commands/flush_command.cpp
  src/commands/search_command.cpp
)

target_link_libraries(tencent-cli
  PRIVATE
    tencent::core
    tencent::third_party_headers   # CLI11
    tencent::project_options
)

install(TARGETS tencent-cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
