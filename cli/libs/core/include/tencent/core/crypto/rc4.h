// rc4.h — RC4 với KSA dùng khoá 8 byte LẶP VÒNG.
//
// Port của ../analytics/app/energy_port.py:85-110 (libqimei.so sub_116750).
//
// Khác RC4 chuẩn ở đúng một chỗ: vòng KSA lấy `key[i & 7]` thay vì
// `key[i % key.size()]`. Nghĩa là chỉ 8 byte ĐẦU của khoá có tác dụng — mọi
// khoá trong luồng energy đều dài đúng 8 byte. Tên hàm nói rõ điều này để
// không ai vô tình dùng như RC4 phổ thông.
#pragma once

#include <string>

namespace tencent::crypto {

// Ném std::invalid_argument nếu khoá rỗng. `data` rỗng thì trả về rỗng.
std::string rc4_cyclic8(const std::string& key, const std::string& data);

}  // namespace tencent::crypto
