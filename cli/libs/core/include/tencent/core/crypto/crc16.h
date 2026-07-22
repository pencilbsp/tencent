// crc16.h — "CRC16" của native: 16 bit CAO của CRC-32 (zlib).
//
// Port của ../analytics/app/energy_port.py:245-251.
//
// Đừng thay bằng CRC-16 chuẩn (CCITT/MODBUS…): native tính CRC-32 theo đúng
// đa thức/khởi tạo của zlib rồi lấy nửa trên. Ta gọi thẳng zlib crc32() nên
// khớp tuyệt đối với `zlib.crc32` bên Python.
#pragma once

#include <string>

namespace tencent::crypto {

// Trả 2 byte theo thứ tự BIG-ENDIAN của (crc32(data) >> 16).
std::string crc16_high_bytes(const std::string& data);

}  // namespace tencent::crypto
