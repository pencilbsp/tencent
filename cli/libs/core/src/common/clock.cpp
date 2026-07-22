// clock.cpp — Xem include/tencent/core/common/clock.h.
#include "tencent/core/common/clock.h"

#include <chrono>
#include <random>

namespace tencent::util {

RequestClock read_request_clock() {
  // Dùng system_clock thay cho gettimeofday() để không phải include <sys/time.h>
  // (lõi phải biên dịch được cả trên Windows). Độ phân giải micro giây là đủ:
  // đây chính là tv_sec/tv_usec mà bản Python đọc.
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

  RequestClock clock;
  clock.now_ms = static_cast<uint64_t>(micros / 1000);
  clock.usec = static_cast<uint32_t>(micros % 1'000'000);

  // Gieo hạt từ chính mốc thời gian, đúng như bản cũ (tv_sec ^ tv_usec).
  const auto seconds = micros / 1'000'000;
  std::mt19937 generator(static_cast<unsigned>(seconds ^ clock.usec));
  clock.rnd = std::uniform_int_distribution<uint32_t>(0, 2'147'483'647u)(generator);
  return clock;
}

int64_t now_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

uint64_t now_millis() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}

uint64_t monotonic_nanos() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace tencent::util
