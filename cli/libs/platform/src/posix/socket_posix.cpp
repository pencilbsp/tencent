// socket_posix.cpp — Hiện thực POSIX của include/tencent/platform/socket.h.
//
// Chuyển nguyên hành vi từ bản cũ `src/proxy.cpp` (connect_tcp/send_all/
// recv_exact + listener loopback) ra khỏi lõi nghiệp vụ. Logic không đổi:
// connect không chặn + poll(POLLOUT) để áp timeout, rồi khôi phục cờ blocking.
#include "tencent/platform/socket.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <stdexcept>

namespace tencent::platform {

void socket_startup() {
  // POSIX không cần khởi tạo; hàm tồn tại để mã gọi không cần #ifdef.
}

SocketHandle connect_tcp(const std::string& host, uint16_t port, long timeout_sec) {
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;          // chấp nhận cả IPv4 lẫn IPv6
  addrinfo* resolved = nullptr;
  const std::string service = std::to_string(port);
  const int rc = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &resolved);
  if (rc != 0) {
    throw std::runtime_error("Phân giải DNS thất bại: " + std::string(::gai_strerror(rc)));
  }

  int fd = -1;
  for (addrinfo* candidate = resolved; candidate; candidate = candidate->ai_next) {
    fd = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
    if (fd < 0) continue;

    // Đặt non-blocking để connect() trả ngay, rồi tự áp deadline bằng poll().
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int connected = ::connect(fd, candidate->ai_addr, candidate->ai_addrlen);
    if (connected != 0 && errno == EINPROGRESS) {
      pollfd wait_fd{fd, POLLOUT, 0};
      const auto deadline = std::chrono::steady_clock::now() +
                            std::chrono::seconds(std::max(1L, timeout_sec));
      do {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0) {
          connected = -1;
          errno = ETIMEDOUT;
          break;
        }
        const int poll_result = ::poll(&wait_fd, 1, static_cast<int>(remaining.count()));
        if (poll_result > 0) {
          int socket_error = 0;
          socklen_t error_size = sizeof(socket_error);
          if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) == 0 &&
              socket_error == 0) {
            connected = 0;
          } else {
            connected = -1;
            errno = socket_error;
          }
          break;
        }
        if (poll_result == 0) {
          connected = -1;
          errno = ETIMEDOUT;
          break;
        }
      } while (errno == EINTR);
    }

    if (flags >= 0) ::fcntl(fd, F_SETFL, flags);  // trả lại chế độ blocking
    if (connected == 0) break;
    ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(resolved);

  if (fd < 0) throw std::runtime_error("Không kết nối được tới " + host);
  return fd;
}

SocketHandle listen_loopback(uint16_t& out_port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error("Không tạo được socket cục bộ");

  const int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // chỉ 127.0.0.1
  address.sin_port = 0;                              // hệ thống tự cấp cổng
  if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      ::listen(fd, 1) != 0) {
    ::close(fd);
    throw std::runtime_error("Không mở được cổng cục bộ");
  }

  socklen_t size = sizeof(address);
  if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
    ::close(fd);
    throw std::runtime_error("Không đọc được cổng cục bộ vừa mở");
  }
  out_port = ntohs(address.sin_port);
  return fd;
}

SocketHandle accept_connection(SocketHandle listener) {
  sockaddr_storage peer{};
  socklen_t peer_size = sizeof(peer);
  const int fd = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_size);
  return fd < 0 ? kInvalidSocket : fd;
}

bool send_all(SocketHandle socket, const uint8_t* data, std::size_t size) {
  while (size > 0) {
#ifdef MSG_NOSIGNAL
    const ssize_t n = ::send(socket, data, size, MSG_NOSIGNAL);
#else
    const ssize_t n = ::send(socket, data, size, 0);
#endif
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    data += n;
    size -= static_cast<std::size_t>(n);
  }
  return true;
}

bool recv_exact(SocketHandle socket, uint8_t* data, std::size_t size) {
  while (size > 0) {
    const ssize_t n = ::recv(socket, data, size, 0);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    data += n;
    size -= static_cast<std::size_t>(n);
  }
  return true;
}

long recv_some(SocketHandle socket, uint8_t* data, std::size_t size) {
  for (;;) {
    const ssize_t n = ::recv(socket, data, size, 0);
    if (n < 0 && errno == EINTR) continue;
    return static_cast<long>(n);
  }
}

void shutdown_write(SocketHandle socket) {
  if (socket != kInvalidSocket) ::shutdown(socket, SHUT_WR);
}

void shutdown_both(SocketHandle socket) {
  if (socket != kInvalidSocket) ::shutdown(socket, SHUT_RDWR);
}

void close_socket(SocketHandle socket) {
  if (socket != kInvalidSocket) ::close(socket);
}

}  // namespace tencent::platform
