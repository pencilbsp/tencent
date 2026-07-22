// process.h — Chạy tiến trình con và thu stdout/stderr NHỊ PHÂN.
//
// Vì sao cần trừu tượng hoá: bản cũ (`src/adb.cpp`) gọi thẳng `popen()` với
// một chuỗi shell tự nối. Cách đó (a) chỉ chạy trên POSIX, (b) mở đường
// shell-injection nếu serial thiết bị hoặc đường dẫn chứa ký tự lạ.
//
// API này nhận danh sách đối số RỜI và exec trực tiếp — KHÔNG qua /bin/sh —
// nên không còn khái niệm "quote cho shell host". Chuỗi lệnh chạy trên THIẾT BỊ
// (ví dụ `su -c "cat ..."`) vẫn là một đối số nguyên vẹn, do shell của Android
// tự diễn giải.
//
// Hiện thực: src/posix/process_posix.cpp (fork+execvp+pipe),
//            src/windows/process_windows.cpp (chưa hỗ trợ, trả lỗi rõ ràng).
#pragma once

#include <string>
#include <vector>

namespace tencent::platform {

// Kết quả một lần chạy. `stdout_data`/`stderr_data` là dữ liệu THÔ (nhị phân
// an toàn — dump MMKV có byte 0x00, không được dùng chuỗi C).
struct ProcessResult {
  int exit_code = -1;        // -1 = không spawn được / bị tín hiệu giết
  std::string stdout_data;
  std::string stderr_data;
  bool spawned = false;      // false = không tìm thấy chương trình

  // Quy ước tiện dụng: thành công = spawn được VÀ exit code 0.
  bool ok() const { return spawned && exit_code == 0; }
};

// Chạy `program` với `args` (KHÔNG qua shell) và đợi kết thúc.
// timeout_ms <= 0 nghĩa là đợi vô hạn. Quá hạn: tiến trình bị SIGKILL, trả về
// dữ liệu đã đọc được kèm exit_code = -1.
// Hàm KHÔNG ném ngoại lệ; lỗi được phản ánh qua ProcessResult.
ProcessResult run_process(const std::string& program,
                          const std::vector<std::string>& args,
                          int timeout_ms = 0);

// Chương trình có tồn tại trên PATH không (dùng để báo lỗi "chưa cài adb"
// thay vì để lệnh thất bại một cách khó hiểu).
bool program_exists(const std::string& program);

}  // namespace tencent::platform
