# tencent-cli (C++)

Công cụ dòng lệnh cho Tencent Video: lấy credential từ thiết bị, làm mới phiên,
lấy URL phát (`getvinfo`) và tải/giải mã video. Thuật toán được port từ bản
Python tham chiếu trong `../analytics/app/`.

> ⚠️ Trước khi code: đọc [AGENTS.md](AGENTS.md). Luật cốt lõi: **mọi thuật toán
> phải pass test byte-exact với dữ liệu capture thật** trước khi commit.
> Về cách chia tầng: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Cấu trúc

```
cli/
├── AGENTS.md               # Luật bắt buộc cho mọi agent (ĐỌC TRƯỚC)
├── CMakeLists.txt          # chỉ lắp ráp; chi tiết build nằm ở cmake/
├── cmake/                  # ProjectOptions / Dependencies / FFmpeg
├── libs/
│   ├── platform/           # tencent::platform — tiến trình con, đường dẫn, socket
│   │   ├── include/tencent/platform/
│   │   └── src/{posix,windows,common}/
│   └── core/               # tencent::core — TOÀN BỘ nghiệp vụ (GUI dùng lại được)
│       ├── include/tencent/core/
│       │   ├── common/     # Bytes, OrderedKV, Result<T>, hex, chuỗi, clock, log
│       │   ├── crypto/     # QQ-TEA, poly131, base64 riêng, XOR, MD5, RC4, ChaCha20…
│       │   ├── codec/      # protobuf tối giản
│       │   ├── net/        # HTTP + gzip, proxy Shadowsocks
│       │   ├── model/      # Account, SessionMaterial, vật liệu Turing (dữ liệu thuần)
│       │   ├── protocol/   # trpc/ auth/ identity/ playback/ — dựng & parse request
│       │   ├── storage/    # bố cục ~/.tencent, đọc/ghi account bundle
│       │   ├── device/     # adb + Tencent Helper
│       │   ├── download/   # HLS → DRM decrypt → remux
│       │   └── service/    # MẶT TIỀN cho giao diện (Account/Playback/Download)
│       └── src/            # phản chiếu cây include
├── apps/cli/               # executable tencent-cli: parse tham số + in kết quả
│   └── src/commands/       # mỗi lệnh con một file
├── third_party/            # submodule: CLI11 v2.4.2, json v3.11.3, ffmpeg n8.1.1
├── tests/                  # ctest, phản chiếu cấu trúc libs/core
│   ├── vectors/            # gen_vectors*.py + generated_vectors*.h
│   └── fixtures/           # dữ liệu tổng hợp, KHÔNG chứa bí mật
└── docs/ARCHITECTURE.md
```

## Build & test

`third_party/` là git submodule (CLI11, nlohmann/json, ffmpeg). Sau khi clone
repo lần đầu — hoặc khi CMake báo thiếu thư mục — chạy:

```bash
git submodule update --init --recursive
```

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Install vào thư mục binary của user để gọi trực tiếp từ terminal:

```bash
cmake --install build --prefix "$HOME/.local"
tencent-cli --help
```

Build bản standalone để chia sẻ trên máy cùng kiến trúc:

```bash
cmake -S . -B build-standalone \
  -DCMAKE_BUILD_TYPE=Release \
  -DTENCENT_CLI_STANDALONE=ON
cmake --build build-standalone -j
ctest --test-dir build-standalone --output-on-failure
cmake --install build-standalone --prefix "$PWD/dist/tencent-cli"
```

Bản standalone link tĩnh OpenSSL và FFmpeg; máy nhận không cần Homebrew hay
các thư viện này. Những lệnh `auth --adb` vẫn cần executable `adb` trong
`PATH`. Binary macOS phải được build riêng cho từng kiến trúc (`arm64` hoặc
`x86_64`. Build mặc định nhắm macOS 13.3 trở lên và tải source OpenSSL 3.6.2
chính thức, kiểm tra SHA-256 trước khi biên dịch.

Sinh lại vector (khi đổi input/khóa/layout): `python3 tests/vectors/gen_vectors.py`.

### Auth qua Android helper

`auth --adb` không tự cài APK và không tự gọi `adb shell su`. Người dùng phải
cài/cập nhật helper trước:

```bash
adb -s SERIAL install -r ../analytics/app/android_helpers/turingprobe/build/turingprobe.apk
./build/apps/cli/tencent-cli auth --adb SERIAL
```

CLI gửi request đến helper đang chờ trên thiết bị. Nếu cấp root cho helper, nó
đọc private credential cùng K26/full K34 raw material. Nếu không root, helper
chỉ trả device snapshot; CLI tái dùng K26 đã lưu hoặc synth/persist một K26 mới
và dựng K34 fallback. Lần tạo account đầu tiên vẫn cần root để đọc credential.

Mỗi account được lưu thành thư mục:

```text
~/.tencent/accounts/<vuid>/
├── auth.json         # account, credential, session, profile
└── fingerprint.json  # raw device snapshot, qimei/guid, raw K26/K34 material
```

CLI vẫn đọc được file monolith `<vuid>.json` cũ và tự ghi bundle ở lần cập nhật
kế tiếp. Device và Turing values được dựng lại từ dữ liệu raw trong
`fingerprint.json`, không lưu lặp các giá trị đã tính.

`getvinfo` không đọc `session_template.json`. cKey envelope, ordered request
layout và static capability được dựng từ constants; device, identity, Cookie và
credential được dựng từ account bundle.

### Tải trực tiếp từ getvinfo

Truyền `-o` để nối request `getvinfo`, tải segment, local transform, DRM
decrypt và remux trong một lệnh:

```bash
./build/apps/cli/tencent-cli getvinfo --vid VIDEO_ID --defn uhd -o video.mp4
```

Capability có thể override trực tiếp, ví dụ:

```bash
./build/apps/cli/tencent-cli getvinfo --vid VIDEO_ID --enable-vvc --enable-hdr
./build/apps/cli/tencent-cli getvinfo --vid VIDEO_ID --enable-drm --enable-dolby both
```

VVC, HDR, Dolby và DRM mặc định đều tắt; chỉ được quảng bá khi có cờ
`--enable-*` tương ứng.

Cache root mặc định nằm tại `~/.tencent`; có thể đổi bằng `--work-dir`.
Mỗi video/chất lượng có namespace riêng để không ghi đè lẫn nhau:

```text
~/.tencent/videos/<vid>/<defn>/
├── capture/      # request.txt + response.json
├── hls/          # playlist, segment và metadata
├── drm/          # segment đã decrypt + merged.ts
└── decoded.json
```

Xoá cache video bằng `flush`; lệnh này chỉ xoá `videos/`, không đụng
`accounts/`:

```bash
./build/apps/cli/tencent-cli flush                         # xoá toàn bộ video cache
./build/apps/cli/tencent-cli flush --vid VIDEO_ID          # xoá mọi quality của video
./build/apps/cli/tencent-cli flush --vid VIDEO_ID --defn uhd
./build/apps/cli/tencent-cli flush --dry-run               # xem trước đường dẫn sẽ xoá
```

Khi response có local key và segment tải về chưa phải MPEG-TS chuẩn, CLI áp
local transform ngay trước khi ghi cache. Segment `/getPlayTs/` được tách khỏi
worker pool, tải tuần tự đúng thứ tự và nghỉ 10 giây giữa hai lần tải; các
segment thường vẫn tải song song theo `--workers`.

DRM native decrypt tự nhận diện video PID/codec từ PMT hoặc Annex-B payload và
hỗ trợ H.264/AVC, H.265/HEVC và H.266/VVC. Ngoài MPEG-TS, CLI cũng xử lý HLS
fMP4 có `EXT-X-MAP` bằng cách local-transform init/media fragment, decrypt VVC
length-prefixed NAL trong `moof/mdat`, rồi remux ra MP4/MOV với `avc1`, `hvc1`
hoặc `vvc1` tương ứng.

Trước khi tải, CLI gọi cấu hình DownloadProxy `data_transport/tp_proxy` bằng
`download_proxy_version`, `platform`, `guid`, `qimei36` và model của account.
Response gốc được lưu tại
`videos/<vid>/<defn>/download_proxy_config.json`. URL playlist/segment được bổ
sung `cost=low`, `cdn_strategy=<CDNStrategy>` và
`dlipscene=10` khi `video.feature & 4`, ngược lại là `0`; query đã có từ server
được giữ nguyên. `download_proxy_version` phải được APK helper đọc từ
`libDownloadProxy.so`, không dùng version hardcode.

FFmpeg tag `n8.1.1` trong submodule `third_party/ffmpeg` được CMake build thành static
`libavformat`, `libavcodec`, `libavutil` và link trực tiếp vào CLI. Runtime
không gọi executable `ffmpeg` và không cần FFmpeg trong `PATH`.

`tencent-cli getvinfo` hỗ trợ proxy thông qua `--proxy <url>`. Với Shadowsocks,
dùng URI Modern Format SIP002:

```bash
./build/apps/cli/tencent-cli getvinfo --vid VIDEO_ID \
  --proxy 'ss://BASE64URL_METHOD_PASSWORD@server.example:8388#profile'
```

Các cipher Shadowsocks được hỗ trợ trực tiếp: `aes-128-gcm`, `aes-256-gcm`,
`chacha20-ietf-poly1305`. URI có SIP003 plugin hoặc cipher AEAD-2022 hiện bị từ
chối với thông báo cụ thể. `http://`, `https://`, `socks4[a]://` và
`socks5[h]://` được chuyển thẳng cho libcurl.

### Tìm kiếm (ẩn danh)

`search` gọi đúng API mà trang tìm kiếm của app dùng khi chưa đăng nhập
(`pbaccess.video.qq.com`, HTTP + JSON) nên **không cần account**. `cid=` là mã
cover (series), `vid=` là mã video lẻ — dùng cho `getvinfo --cid/--vid`.

```bash
./build/apps/cli/tencent-cli search 庆余年                       # trang đầu
./build/apps/cli/tencent-cli search 庆余年 --episodes            # kèm dải tập
./build/apps/cli/tencent-cli search 庆余年 --page 1 --pre-qid <qid>   # trang kế (qid in ở cuối)
./build/apps/cli/tencent-cli search 庆余 --suggest               # gợi ý khi gõ
```

Mã nằm ở `libs/core/.../protocol/search/` (dựng body + parse) và
`service/search_service.h` (mặt tiền, `Result<T>`); test byte-exact:
`tests/protocol/test_search_api.cpp`, ground-truth sinh bằng
`tests/vectors/gen_vectors_search.py` từ `../analytics/app/search_web_client.py`.

## Ranh giới Port vs Dump

Xem [AGENTS.md §3](AGENTS.md). Tóm tắt: thuật toán (cKey, crypto, body builder,
flowid, cookie/adpass refresh) thì PORT; trạng thái thiết bị/tài khoản
(`qimei`, `guid`, credential...) thì DUMP vào `auth.json`/`fingerprint.json`.
Build constants như app version, request URL, cKey envelope, TEA/XOR key,
package/cert và registry Field[16] nằm trong `getvinfo_constants`. cKey TLV base
được dựng trong C++ từ constants + `appVer/guid/qimei36/devicemodel`.

## Tiến độ

Xem bảng trạng thái trong [AGENTS.md §7](AGENTS.md).

## Ghi chú khi nâng cấp từ bản v1

- Executable phụ `tencent_refresh` (đọc `refresh_session.json` do helper Python
  ghi) đã bị bỏ. Chức năng làm mới phiên nằm ở `tencent-cli auth --refresh`,
  lấy credential trực tiếp từ account bundle.
- Binary nằm ở `build/apps/cli/tencent-cli` thay vì `build/tencent-cli`.
- Thư mục dữ liệu `~/.tencent` và định dạng file KHÔNG đổi.
