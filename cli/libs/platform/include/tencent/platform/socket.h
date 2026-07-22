// socket.h — Socket TCP tối thiểu cho cầu nối Shadowsocks cục bộ.
//
// Chỉ chứa đúng những thao tác mà `net/proxy.cpp` cần: kết nối ra ngoài có
// timeout, mở cổng nghe trên loopback, accept một kết nối, gửi/nhận thô và
// đóng nửa chiều. Không phải thư viện mạng đa dụng — cố ý giữ hẹp.
//
// Hiện thực: src/posix/socket_posix.cpp, src/windows/socket_windows.cpp.
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace tencent::platform {

#if defined(_WIN32)
using SocketHandle = uintptr_t;                       // SOCKET của Winsock
inline constexpr SocketHandle kInvalidSocket = static_cast<SocketHandle>(~0ull);
#else
using SocketHandle = int;                             // file descriptor
inline constexpr SocketHandle kInvalidSocket = -1;
#endif

// Khởi tạo tầng socket (Winsock cần WSAStartup). POSIX: không làm gì.
// Gọi bao nhiêu lần cũng được; tự đảm bảo chỉ khởi tạo một lần.
void socket_startup();

// Kết nối TCP tới host:port, có timeout cho bước connect (giây, tối thiểu 1).
// Thử lần lượt mọi bản ghi getaddrinfo (IPv4 lẫn IPv6).
// Ném std::runtime_error nếu DNS hỏng hoặc không kết nối được.
SocketHandle connect_tcp(const std::string& host, uint16_t port, long timeout_sec);

// Mở socket nghe trên 127.0.0.1 với cổng do hệ thống cấp (backlog 1).
// Cổng thực tế được ghi vào `out_port`. Ném std::runtime_error nếu thất bại.
SocketHandle listen_loopback(uint16_t& out_port);

// Chờ và nhận một kết nối. Trả kInvalidSocket nếu socket nghe đã bị đóng.
SocketHandle accept_connection(SocketHandle listener);

// Gửi trọn `size` byte (lặp cho tới hết, bỏ qua EINTR).
// Trả false nếu phía kia đóng hoặc lỗi. Không phát SIGPIPE.
bool send_all(SocketHandle socket, const uint8_t* data, std::size_t size);

// Nhận đúng `size` byte. false nếu EOF/lỗi trước khi đủ.
bool recv_exact(SocketHandle socket, uint8_t* data, std::size_t size);

// Nhận tối đa `size` byte. >0 số byte đọc được; 0 = EOF; <0 = lỗi.
long recv_some(SocketHandle socket, uint8_t* data, std::size_t size);

void shutdown_write(SocketHandle socket);      // hết dữ liệu gửi (nửa chiều)
void shutdown_both(SocketHandle socket);       // đánh thức luồng đang chờ
void close_socket(SocketHandle socket);

}  // namespace tencent::platform
