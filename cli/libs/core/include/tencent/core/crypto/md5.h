// md5.h — MD5 (RFC 1321), tự port.
//
// Vì sao không dùng OpenSSL: bản OpenSSL đánh dấu MD5 là deprecated và trong
// một số cấu hình FIPS thì từ chối chạy, trong khi thuật toán `energy` BẮT BUỘC
// phải cho ra đúng digest đó. Tự port ⇒ kết quả không phụ thuộc cấu hình máy.
//
// Nguồn dùng đến: ../analytics/app/energy_port.py:304 (A1) và :312 (pass B).
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace tencent::crypto {

// Digest 16 byte thô.
std::array<uint8_t, 16> md5_digest(const std::string& message);

// Digest dạng chuỗi 16 byte (tiện nối vào buffer nhị phân).
std::string md5(const std::string& message);

}  // namespace tencent::crypto
