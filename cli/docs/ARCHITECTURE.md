# Kiến trúc

Mục tiêu của bản v2: **lõi nghiệp vụ tách rời hoàn toàn khỏi giao diện**, để hôm
nay dùng cho CLI và mai dùng lại nguyên vẹn cho GUI.

```
cli/
├── CMakeLists.txt          chỉ lắp ráp — không chứa logic build chi tiết
├── cmake/                  ProjectOptions / Dependencies / FFmpeg
├── third_party/            CLI11, nlohmann/json, ffmpeg (vendor)
├── libs/
│   ├── platform/           tencent::platform — trừu tượng hoá hệ điều hành
│   └── core/               tencent::core     — TOÀN BỘ nghiệp vụ
├── apps/cli/               tencent-cli       — parse tham số + in kết quả
├── tests/                  ctest, phản chiếu cấu trúc libs/core
└── docs/
```

## Ba tầng, một chiều phụ thuộc

```
apps/cli  ──►  libs/core  ──►  libs/platform  ──►  hệ điều hành
                    │
                    └──►  third_party (header-only) + zlib/curl/openssl/ffmpeg
```

Chiều mũi tên là **một chiều tuyệt đối**. Cụ thể:

| Tầng | Được phép | KHÔNG được phép |
|---|---|---|
| `apps/cli` | đọc argv, in ra terminal, gọi `tencent::service::*` | chứa thuật toán, tự gọi HTTP/đĩa |
| `libs/core` | mọi nghiệp vụ, ném ngoại lệ nội bộ | `std::cout`, `argv`, `std::exit`, `#include <unistd.h>` |
| `libs/platform` | syscall, `<filesystem>`, socket, tiến trình con | biết gì về Tencent/giao thức |

Hai quy tắc dễ kiểm tra bằng `grep`:

```bash
grep -rn "std::cout\|std::cerr\|std::exit" libs/core/src   # phải rỗng
grep -rn "unistd.h\|sys/socket.h\|popen"  libs/core/src   # phải rỗng
```

## Bên trong `libs/core`

Thư mục xếp theo tầng phụ thuộc, từ dưới lên:

| Thư mục | Namespace | Nội dung |
|---|---|---|
| `common/` | `tencent`, `tencent::util` | từ vựng chung: `Bytes`, `OrderedKV`, `Result<T>`/`Error`, hex, chuỗi, đồng hồ, JSON, log |
| `crypto/` | `tencent::crypto` | primitive tùy biến của Tencent: QQ-TEA, poly131, Base64 riêng, XOR, MD5, RC4 khoá-8, ChaCha20, CRC16, random Bionic |
| `codec/` | `tencent::pb` | protobuf tối giản (đọc/ghi field giữ thứ tự) |
| `net/` | `tencent::http`, `tencent::proxy` | HTTP + gzip (libcurl/zlib), cầu nối Shadowsocks cục bộ |
| `model/` | `tencent::account`, `tencent::model`, `tencent::turing` | dữ liệu THUẦN: Account, SessionMaterial, vật liệu Turing |
| `protocol/trpc/` | `tencent::trpc` | khung VBPB 16B + qqlive head |
| `protocol/auth/` | `tencent::auth`, `tencent::vip`, `tencent::usercenter` | energy MAC, login-refresh, VIP, User Center |
| `protocol/identity/` | `tencent::qimei`, `tencent::guid`, `tencent::turing` | đăng ký Qimei, xin GUID, dựng K-value Turing |
| `protocol/playback/` | `tencent::playback` | hằng số, capability, cKey v4.2, adpass, ngữ cảnh + bộ dựng request getvinfo |
| `storage/` | `tencent::storage` | bố cục `~/.tencent`, đọc/ghi bundle account |
| `device/` | `tencent::adb`, `tencent::turing` | nói chuyện với thiết bị Android (adb, Tencent Helper) |
| `download/` | `tencent::download`, `tencent::download_proxy` | tải HLS, giải mã DRM, remux MP4 |
| `service/` | `tencent::service` | **mặt tiền cho giao diện** |

### Tầng `service` — điểm vào duy nhất của giao diện

Ba lớp, mỗi lớp gói trọn một luồng nghiệp vụ:

- `AccountService` — liệt kê, lấy dữ liệu từ thiết bị, làm mới phiên, thông tin VIP.
- `PlaybackService` — getvinfo: dựng request, gửi, giải ANC, trích URL phát.
- `DownloadService` — lưu capture, chạy pipeline tải, dọn cache.

Hợp đồng của tầng này:

1. **Không ném ngoại lệ ra ngoài.** Mọi thứ trả về `Result<T>` (`common/result.h`);
   `service/error.h::guard()` bắt ngoại lệ nội bộ và chuyển thành `Error` có mã
   phân loại (`kAuthExpired`, `kNetwork`, `kInvalidArgument`, …).
2. **Không in.** Tiến độ và cảnh báo đi qua sink log (`common/logging.h`).
   CLI cài sink in ra terminal (`apps/cli/src/console.cpp`); GUI cài sink khác.
3. **Không quyết định mã thoát.** `service::exit_code_for(ErrorCode)` là gợi ý
   cho CLI, không phải luật của lõi.

## Viết một giao diện mới (ví dụ GUI)

Chỉ cần link `tencent::core` rồi:

```cpp
tencent::set_log_sink([](tencent::LogLevel level, const std::string& message) {
  // đẩy vào cửa sổ log; message bắt đầu bằng '\r' là cập nhật tiến độ
});

tencent::service::PlaybackService playback;
tencent::service::PlaybackRequest request;
request.vid = "e41027tno61";
if (auto result = playback.fetch(request)) {
  for (const auto& stream : result.value().streams) { /* hiển thị */ }
} else {
  show_error(result.error().message);
}
```

Không cần biết gì về cKey, Turing hay adb.

## `libs/platform`

Bốn header, mỗi cái một loại tài nguyên hệ điều hành:

| Header | Dùng ở đâu |
|---|---|
| `process.h` | `device/adb_bridge.cpp` gọi `adb` (fork+execvp, **không qua shell**) |
| `paths.h` | `storage/paths.cpp` xác định `~/.tencent` |
| `filesystem.h` | mọi chỗ đọc/ghi file (kể cả ghi thay-thế-nguyên-tử) |
| `socket.h` | `net/proxy.cpp` cho cầu nối Shadowsocks |

Nguồn được chọn theo nền tảng trong `libs/platform/CMakeLists.txt`
(`src/posix/` cho macOS + Linux, `src/windows/` cho Windows). Trong lõi **không
có `#ifdef` hệ điều hành nào**. Windows hiện mới có phần `paths`; `process` và
`socket` là khung có chú thích rõ những gì cần điền.

## Kỷ luật byte-exact vẫn giữ nguyên

Việc tái cấu trúc **không đụng vào thân các thuật toán đã verify**. Các file
`crypto/*.cpp`, `playback/ckey_*.cpp`, `auth/login_sign.cpp`, `codec/protobuf.cpp`
được chuyển sang nguyên vẹn; chỉ đổi tên/namespace và bổ sung chú thích nguồn.
Bằng chứng: 15/15 test byte-exact vẫn xanh sau khi chuyển, và body getvinfo sinh
ra trùng khớp bản cũ tới từng byte (chỉ khác các trường phụ thuộc thời gian).

Xem `AGENTS.md` cho quy trình bắt buộc khi sửa thuật toán.
