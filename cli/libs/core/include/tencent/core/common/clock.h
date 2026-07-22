// clock.h — Nguồn thời gian của lõi.
//
// KỶ LUẬT QUAN TRỌNG (AGENTS.md §0): mọi thuật toán byte-exact đều nhận thời
// gian/entropy qua THAM SỐ, không tự gọi đồng hồ bên trong. File này là nơi
// DUY NHẤT lấy giờ hệ thống, để test có thể tái lập kết quả bằng cách truyền
// giá trị cố định.
#pragma once

#include <cstdint>

namespace tencent::util {

// Bộ ba giá trị mà một request cần: mốc mili giây, số ngẫu nhiên và micro giây
// trong giây hiện tại. Ba giá trị PHẢI lấy từ CÙNG một lần đọc đồng hồ
// (../analytics/app/login_refresh.py:176-179 và ghi chú AGENTS.md §5 về việc
// tv_usec/param2/rand phải cùng mốc).
struct RequestClock {
  uint64_t now_ms = 0;   // epoch mili giây
  uint32_t rnd = 0;      // ngẫu nhiên [0, 2^31-1]
  uint32_t usec = 0;     // phần micro giây [0, 1_000_000)
};

// Đọc đồng hồ hệ thống một lần và dẫn xuất cả ba giá trị.
RequestClock read_request_clock();

int64_t now_seconds();     // epoch giây
uint64_t now_millis();     // epoch mili giây
uint64_t monotonic_nanos(); // đồng hồ đơn điệu (dùng cho flowid, đo thời gian)

}  // namespace tencent::util
