// process_posix.cpp — Hiện thực POSIX của include/tencent/platform/process.h.
//
// fork() + execvp() + hai ống (stdout/stderr) thay cho popen():
//   • KHÔNG qua /bin/sh  → không thể bị shell-injection từ serial thiết bị,
//     đường dẫn hay tên gói.
//   • Đọc song song 2 ống bằng poll() → không bị deadlock khi con ghi nhiều
//     ra stderr trong lúc ta còn đang đọc stdout.
//   • Ống "báo lỗi exec" đặt cờ FD_CLOEXEC: nếu execvp thất bại, tiến trình con
//     ghi errno vào ống rồi thoát; ống tự đóng khi exec thành công. Nhờ đó phân
//     biệt được "không tìm thấy chương trình" với "chương trình trả mã 127".
#include "tencent/platform/process.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace tencent::platform {
namespace {

// Đóng fd nếu còn mở và đánh dấu -1 (gọi nhiều lần vô hại).
void close_fd(int& fd) {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

// Đọc tất cả những gì đang sẵn sàng trên fd vào `sink`.
// Trả false khi gặp EOF (phía ghi đã đóng).
bool drain(int fd, std::string& sink) {
  char buffer[8192];
  for (;;) {
    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
    if (n > 0) {
      sink.append(buffer, static_cast<std::size_t>(n));
      continue;             // đọc tiếp cho tới khi hết dữ liệu sẵn sàng
    }
    if (n == 0) return false;                       // EOF
    if (errno == EINTR) continue;                   // bị tín hiệu cắt ngang
    if (errno == EAGAIN || errno == EWOULDBLOCK) return true;  // hết dữ liệu
    return false;                                   // lỗi thật sự
  }
}

}  // namespace

ProcessResult run_process(const std::string& program,
                          const std::vector<std::string>& args, int timeout_ms) {
  ProcessResult result;

  // argv phải dựng XONG trước fork: sau fork trong tiến trình con chỉ được gọi
  // các hàm async-signal-safe, không được cấp phát bộ nhớ.
  std::vector<char*> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char*>(program.c_str()));
  for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  int out_pipe[2] = {-1, -1};
  int err_pipe[2] = {-1, -1};
  int exec_pipe[2] = {-1, -1};
  if (::pipe(out_pipe) != 0) return result;
  if (::pipe(err_pipe) != 0) {
    close_fd(out_pipe[0]);
    close_fd(out_pipe[1]);
    return result;
  }
  if (::pipe(exec_pipe) != 0) {
    close_fd(out_pipe[0]); close_fd(out_pipe[1]);
    close_fd(err_pipe[0]); close_fd(err_pipe[1]);
    return result;
  }
  ::fcntl(exec_pipe[1], F_SETFD, FD_CLOEXEC);  // tự đóng khi exec thành công

  const pid_t pid = ::fork();
  if (pid < 0) {
    close_fd(out_pipe[0]); close_fd(out_pipe[1]);
    close_fd(err_pipe[0]); close_fd(err_pipe[1]);
    close_fd(exec_pipe[0]); close_fd(exec_pipe[1]);
    return result;
  }

  if (pid == 0) {
    // ── Tiến trình con ──────────────────────────────────────────────────────
    ::dup2(out_pipe[1], STDOUT_FILENO);
    ::dup2(err_pipe[1], STDERR_FILENO);
    ::close(out_pipe[0]); ::close(out_pipe[1]);
    ::close(err_pipe[0]); ::close(err_pipe[1]);
    ::close(exec_pipe[0]);
    ::execvp(program.c_str(), argv.data());
    // Chỉ tới đây khi exec THẤT BẠI: báo errno cho cha rồi thoát ngay.
    const int exec_errno = errno;
    ssize_t ignored = ::write(exec_pipe[1], &exec_errno, sizeof(exec_errno));
    (void)ignored;
    ::_exit(127);
  }

  // ── Tiến trình cha ────────────────────────────────────────────────────────
  close_fd(out_pipe[1]);
  close_fd(err_pipe[1]);
  close_fd(exec_pipe[1]);

  int exec_errno = 0;
  const ssize_t exec_read = ::read(exec_pipe[0], &exec_errno, sizeof(exec_errno));
  close_fd(exec_pipe[0]);
  if (exec_read == static_cast<ssize_t>(sizeof(exec_errno))) {
    // execvp hỏng (thường là ENOENT: chưa cài chương trình).
    close_fd(out_pipe[0]);
    close_fd(err_pipe[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
    result.spawned = false;
    result.stderr_data = std::strerror(exec_errno);
    return result;
  }
  result.spawned = true;

  ::fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
  ::fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 0);
  bool timed_out = false;
  bool out_open = true, err_open = true;

  while (out_open || err_open) {
    pollfd fds[2];
    int count = 0;
    if (out_open) fds[count++] = pollfd{out_pipe[0], POLLIN, 0};
    if (err_open) fds[count++] = pollfd{err_pipe[0], POLLIN, 0};

    int wait_ms = -1;  // -1 = chờ vô hạn
    if (timeout_ms > 0) {
      const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline - std::chrono::steady_clock::now());
      if (remaining.count() <= 0) { timed_out = true; break; }
      wait_ms = static_cast<int>(remaining.count());
    }

    const int ready = ::poll(fds, static_cast<nfds_t>(count), wait_ms);
    if (ready < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (ready == 0) { timed_out = true; break; }

    for (int i = 0; i < count; ++i) {
      if (fds[i].revents == 0) continue;
      const bool is_stdout = fds[i].fd == out_pipe[0];
      std::string& sink = is_stdout ? result.stdout_data : result.stderr_data;
      const bool still_open = drain(fds[i].fd, sink) && !(fds[i].revents & POLLHUP);
      if (!still_open) {
        if (is_stdout) out_open = false; else err_open = false;
      }
    }
  }

  if (timed_out) ::kill(pid, SIGKILL);
  close_fd(out_pipe[0]);
  close_fd(err_pipe[0]);

  int status = 0;
  while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  if (!timed_out && WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else {
    result.exit_code = -1;   // bị giết vì quá hạn hoặc do tín hiệu
  }
  return result;
}

bool program_exists(const std::string& program) {
  if (program.find('/') != std::string::npos) {
    return ::access(program.c_str(), X_OK) == 0;
  }
  const char* path_env = std::getenv("PATH");
  if (!path_env) return false;
  const std::string path_list(path_env);
  std::size_t start = 0;
  while (start <= path_list.size()) {
    std::size_t end = path_list.find(':', start);
    if (end == std::string::npos) end = path_list.size();
    const std::string dir = path_list.substr(start, end - start);
    if (!dir.empty()) {
      const std::string candidate = dir + "/" + program;
      if (::access(candidate.c_str(), X_OK) == 0) return true;
    }
    if (end == path_list.size()) break;
    start = end + 1;
  }
  return false;
}

}  // namespace tencent::platform
