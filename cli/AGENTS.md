# AGENTS.md — Quy tắc bắt buộc cho mọi AI agent làm việc trong `cli/`

> Đây là bộ luật của dự án port `getvinfo` (Tencent Video) sang C++.
> ĐỌC TOÀN BỘ trước khi viết bất kỳ dòng code nào. Các quy tắc này
> GHI ĐÈ mọi thói quen mặc định.

---

## 0. Luật vàng — BYTE-EXACT hoặc không tồn tại

**Mọi thuật toán được port PHẢI vượt qua một bài test so khớp BYTE-FOR-BYTE
với dữ liệu ground-truth sinh từ implementation Python gốc (`../analytics/app/*.py`) trên
dữ liệu đã CAPTURE thật từ thiết bị.**

- Không có test byte-exact ⇒ code coi như CHƯA TỒN TẠI, KHÔNG được commit.
- "Chạy ra kết quả trông hợp lý" KHÔNG phải là pass. Chỉ `expected == actual`
  trên từng byte mới là pass.
- Nếu không thể test byte-exact một phần nào đó (vì nó phụ thuộc random/thời
  gian), thì phải **tách phần tất định ra để test**, và inject random/clock qua
  tham số để test được (xem `playback/ckey_codec` — entropy được inject).

## 0.5. Ranh giới tầng — KHÔNG được vi phạm

Chi tiết: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). Ba luật kiểm được bằng grep:

```
apps/cli  ──►  libs/core  ──►  libs/platform
```

1. `libs/core` KHÔNG in ra màn hình và KHÔNG đọc argv.
   Thông điệp đi qua sink log (`common/logging.h`), lỗi đi qua `Result<T>`.
   ```bash
   grep -rn "std::cout\|std::cerr\|std::exit\|std::printf" libs/core/src   # phải rỗng
   ```
2. `libs/core` KHÔNG gọi thẳng hệ điều hành. Tiến trình con, đường dẫn HOME,
   socket, đọc/ghi file đều đi qua `tencent::platform`.
   ```bash
   grep -rn "unistd.h\|sys/socket.h\|popen\|getenv" libs/core/src          # phải rỗng
   ```
3. `apps/cli` KHÔNG chứa nghiệp vụ. Mỗi lệnh con chỉ được: đổ tham số vào một
   `service::*Request`, gọi `tencent::service::*`, rồi in kết quả. Thêm logic
   nghiệp vụ vào `apps/` = sai chỗ, GUI sẽ không dùng lại được.

Khi thêm một module mới, đặt nó vào đúng tầng:
`common` (không phụ thuộc gì) → `crypto`/`codec` → `net` → `model` (dữ liệu
thuần) → `protocol` (dựng/parse request) → `storage`/`device` → `download` →
`service` (mặt tiền). Không được có cạnh ngược chiều.

## 1. Ground-truth sinh ra như thế nào

- Script `tests/vectors/gen_vectors.py` là NGUỒN SỰ THẬT DUY NHẤT cho test vectors.
  Nó import trực tiếp các module Python trong `../app`, nạp capture/cKey thật
  đã capture, **patch `os.urandom` về tất định**, rồi xuất ra
  `tests/vectors/generated_vectors.h` (committed).
- Dữ liệu capture thật dùng làm input:
  - `../analytics/app/request.authdump.txt` — request thật có `cKey=...` (đã pass live,
    `em=0 exem=0`, xem `../analytics/app/getvinfo_param_provenance.md`).
- Quy trình KHÔNG bao giờ được nới lỏng để "cho test xanh". Nếu C++ khác Python,
  **C++ sai** — sửa C++, đừng sửa expected.

## 2. Vòng đời một thay đổi (BẮT BUỘC theo đúng thứ tự)

```
1. Implement / sửa module C++  (kèm comment nguồn — xem §4)
2. Nếu đụng tới input/khóa/layout: chạy  python3 tests/vectors/gen_vectors.py
3. cmake --build build              # build sạch
4. ctest --test-dir build --output-on-failure   # PHẢI xanh 100%
5. CHỈ KHI tất cả test pass byte-exact → git add + git commit
6. Commit message ghi rõ: module nào, test nào pass, vector nào dùng
```

Cấm commit khi có dù chỉ một test đỏ. Cấm `git commit --no-verify` để né test.

## 3. Ranh giới PORT vs DUMP (đọc kỹ — đừng port nhầm)

Tham chiếu: `../analytics/app/getvinfo_param_provenance.md`,
`../analytics/app/plan_getvinfo_params.md` §3 (Group A–F),
`../analytics/app/getvinfo_capability_bits.md`.

### PORT (thuật toán — sinh offline mỗi request/session):
- Crypto: QQ-TEA CBC (biến thể dual-state), poly131, Tencent-Base64, XOR lặp khóa.
- cKey v4.2 encoder (TLV → preamble → pad → QQTEA → checksum → XOR → b64 → `--01`).
- Field[19] patch tv_usec; sinh param2 (Field[20]) / rand (Field[21]).
- flowid generator.
- Body builder + URL-encode kiểu Java (giữ `-_.*`; KHÔNG encode `=`/`&` trong
  `reportparams`/`mediascene`); thứ tự key FIXED.
- Capability bitmask (có thể để dạng hằng số tĩnh).
- Cookie + QAD/adpass envelope generation offline.
- Request context từ constants + account bundle; HTTP POST + giải gzip.

### KHÔNG PORT (dump từ thiết bị, nạp qua file JSON):
- `xor_key` / `tea_key` (hardcode trong libqimei.so) → constants group.
- cKey envelope: preamble, raw block, trailing → constants theo build.
- Group A constants (`spaudio`, `spvideo`, `drm`, `encryptVer=4.2`, …).
- Group B device id: `guid`, `qimei`, `abuserid`, `devicemodel`, `manufacturer`,
  `osver`, `screeninfo`, `platform`, `sdtfrom`, `appVer`, `video_omgid`, …
- Group C auth: `userid`, `openid`, `viptype`, cookie login (`vusession`, …).
  `adpass` is generated from the current Cookie + QAD defaults; do not copy the
  template's original `adpass` JSON.
- qimei mint (oz/oo) và Turing TAID/oaid — cached device/server state.

> Nguyên tắc: trạng thái thiết bị/tài khoản đọc từ account bundle
> `auth.json` + `fingerprint.json`; build state nằm trong constants.
> cKey TLV base không đọc từ template field dump nữa; C++ dựng từ constants +
> device/session values (`appVer`, `guid`, `qimei36`, `devicemodel`) rồi ghi đè
> các field per-request. `app_ver`, request URL, TEA/XOR key và registry IDs nằm
> trong constants group.

## 4. Comment nguồn — BẮT BUỘC cho mọi util/helper

Mỗi hàm util/helper/crypto PHẢI có comment chỉ rõ NGUỒN và TÀI LIỆU:
- File + dòng Python gốc nó port từ đó, ví dụ:
  `// Port của parse_ckey_v42.py:98-103 (poly131)`
- Tài liệu .md tham chiếu nếu có, ví dụ:
  `// Xem ../analytics/app/getvinfo_capability_bits.md §drm`
- Offset IDA / hàm native nếu liên quan, ví dụ:
  `// libtvkplayer.so tvk_vinfo_build_request @ 0x4D0E08`

Không có comment nguồn ⇒ coi như chưa hoàn thành.

## 5. Bẫy kỹ thuật khi port (đã biết trước)

- QQ-TEA dùng từ 32-bit BIG-ENDIAN + tràn mod 2^32. Dùng `uint32_t` (tự wrap),
  `>>` là dịch phải logic trên unsigned. `s` khởi tạo `DELTA*16` khi decrypt.
- `pad_count = (-2 - len(body)) % 8` — Python modulo luôn ≥ 0; trong C++ phải
  `((-2 - (int)len) % 8 + 8) % 8`.
- param2 (Field[20]) dài đúng 29 ký tự = 13 (ts_ms) + 16 (hex 8 byte). rand21
  (Field[21]) = 16 ký tự. tv_usec, param2, rand phải lấy CÙNG một mốc thời gian.
- Body giữ THỨ TỰ chèn key — dùng vector<pair>/ordered map, KHÔNG `std::map`.
- Field[31] (cert chain) để RỖNG (build hiện tại chấp nhận rỗng).
- Hex/secret: in ra để debug có thể bị tool làm sai lệch — verify trong test
  (so sánh bằng code), đừng tin mắt nhìn hex trên terminal.
- DECODE cKey: vùng pad (`plain[3 : 3+pad_count]`) trong cKey CAPTURE THẬT là
  NGẪU NHIÊN (vd 0x21), KHÁC encoder của ta (ghi \x00). Nên cKey capture KHÔNG
  encode-lại byte-exact được; decode vẫn đúng (bỏ qua pad). ⇒ test decode so với
  parse_full_captured; test roundtrip dùng cKey do CHÍNH encoder ta sinh (pad=0).

## 6. Git

- Repo git nằm NGAY trong `cli/` (độc lập với repo cha — repo cha không phải git).
- Mỗi commit = một đơn vị công việc đã PASS test byte-exact.
- `tests/vectors/generated_vectors*.h` được commit (tái sinh được qua `gen_vectors*.py`).
- KHÔNG commit thư mục `build/` (xem `.gitignore`).

## 7. Trạng thái hiện tại

### Kiến trúc (v2 — nhánh `v2`)

Toàn bộ mã nguồn đã được tái cấu trúc thành ba tầng
(`apps/cli` → `libs/core` → `libs/platform`, xem [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)).
Thân các thuật toán đã verify KHÔNG bị viết lại — chỉ đổi vị trí/namespace và bổ
sung chú thích nguồn. Bằng chứng giữ nguyên hành vi:

- 15/15 test byte-exact xanh sau khi chuyển;
- body `getvinfo` sinh ra trùng bản v1 tới từng byte (76 tham số, cùng thứ tự;
  chỉ khác `flowid`/`cKey`/`ctime` là các trường theo thời gian).

Thay đổi có ảnh hưởng tới người dùng:

- executable phụ `tencent_refresh` + module `refresh_session` (đọc
  `refresh_session.json` do helper Python ghi) ĐÃ BỎ — dùng
  `tencent-cli auth --refresh`, lấy credential từ account bundle;
- binary nằm ở `build/apps/cli/tencent-cli`;
- `~/.tencent` và định dạng file không đổi.

### Bảng module

| Module | Vị trí | Trạng thái |
|---|---|---|
| crypto (QQ-TEA/poly131/base64/xor/MD5/RC4/ChaCha20/CRC16/random) | `libs/core/src/crypto/` | ✅ byte-exact |
| cKey v4.2 encode+decode + Field[15]/[16]/[19] | `protocol/playback/ckey_*` | ✅ byte-exact |
| hằng số Group A + CapabilitySet | `protocol/playback/{constants,capability}` | ✅ byte-exact |
| body builder getvinfo (flowid, cKey, encode_body) | `protocol/playback/request_builder` | ✅ byte-exact |
| protobuf + khung TRPC/VBPB | `codec/protobuf`, `protocol/trpc/head` | ✅ byte-exact |
| energy MAC + login-refresh | `protocol/auth/login_{sign,refresh}` | ✅ byte-exact |
| VIP / User Center | `protocol/auth/{vip_info,user_center}` | ✅ byte-exact |
| Qimei REGISTER / GUID / Turing K-values | `protocol/identity/` | ✅ byte-exact |
| account bundle (parse + I/O) | `model/account`, `storage/` | ✅ test parse |
| adb + Tencent Helper | `device/` | dump — không byte-exact |
| tải HLS/DRM/remux | `download/` | ✅ test cache/parse, verify live |
| mặt tiền cho giao diện | `service/` | ✅ có test gián tiếp qua CLI |

Nhánh R (login-refresh vusession): port ../analytics/app/energy_port.py (energy MAC:
RC4/ChaCha20/MD5/CRC16/Bionic-random + nonce/fold/pass-B) → `login_sign`; và
../analytics/app/login_refresh_proto.py + login_refresh_response.py + login_refresh.py
(build_refresh_frame) → `login_refresh`. Ground-truth RIÊNG
`tests/vectors/gen_vectors_sign.py` → `tests/vectors/generated_vectors_sign.h` (namespace `vsign`),
KHÔNG đụng gen_vectors.py (drift). 25 energy case (usec inject) + body + frame
đầy đủ (C++ tự tính sign khớp Python truyền sign tường minh) + parse response.

Nhánh TOOL (tencent-cli): executable hợp nhất `tencent-cli`. Subcommand đầu
`auth --adb <device_id>` yêu cầu người dùng cài Tencent Helper trước. CLI gửi
explicit broadcast đến helper và đọc response private bằng `run-as`; CLI không
tự cài APK và không gọi `adb shell su`. Nếu helper được cấp root, helper copy
nhóm A(identity)+B(credential)+C(session), K26 cache và raw state cho full K34
để CLI parse/decrypt. Nếu không root, private fields để trống; CLI tái dùng hoặc
synth/persist K26 và dựng K34 fallback. Account đầu tiên vẫn cần private
credential; non-root chỉ cập nhật được account đã lưu.
Kết quả được lưu theo bundle schema v2:
`~/.tencent/accounts/<vuid>/auth.json` chứa account/login/session và
`fingerprint.json` chứa exact `turing-raw/1`, qimei/guid, K26 blobs, Turing prefs
và K34 CL raw. Device/K26/K34 được dựng lại từ raw khi load; file monolith
`<vuid>.json` schema v1 vẫn đọc được và được chuyển sang bundle ở lần ghi kế
tiếp. `config.json` giữ default account; backup cũng là bundle. Module: `model/account` (parse cookie/MMKV, to_session) + `storage/account_store`
(bundle/legacy trên đĩa), `device/helper_probe` (IPC với helper, dựng vật liệu
Turing từ ảnh chụp), `model/turing_material` (giải mã K26/K34 raw). Test
`account_parse` kiểm tra tách auth/fingerprint và hydrate raw offline.
Device KHÔNG còn hardcode: `to_session()` phải cấp đầy đủ model/app/platform/
brand/OS/API/screen; `trpc_head` từ chối request nếu thiếu.

Nhánh DOWNLOAD: `tencent-cli getvinfo ... -o output.mp4` tự lưu request/response
capture rồi chạy pipeline tải HLS, local transform, DRM decrypt và remux.
Cache download được tách theo `~/.tencent/videos/<vid>/<defn>/`, bên trong mới
có `capture/`, `hls/`, `drm/` và `decoded.json`; các account vẫn ở thư mục gốc.
Segment có local key được transform ngay khi payload chưa phải TS chuẩn.
Native DRM decrypt nhận diện video stream từ PMT/Annex-B và hỗ trợ H.264/AVC,
H.265/HEVC, H.266/VVC với cùng txsdd protected-byte pattern.
`/getPlayTs/` luôn chạy tuần tự theo playlist, nghỉ 10 giây giữa hai download
thật và retry; segment CDN thường dùng worker pool. FFmpeg official tag
`n8.1.1` được build thành static `libavformat/libavcodec/libavutil` và link vào
`getvinfo_core`; CLI không spawn binary `ffmpeg`. Live verify ngày 2026-06-13:
`e41027tno61`, `em=0/exem=0`, HEVC Main 10 3840x1606 + AAC, 2775.760 giây.

Nhánh TEMPLATE-FREE: `tencent-cli getvinfo` không có loader/file template và
không có cờ `--template`. Ordered params,
cKey envelope/raw/trailing và capability lấy từ constants; Cookie được dựng lại
từ account bundle với timestamp mới. Live verify ngày 2026-06-13:
`e41027tno61`, `em=0/exem=0`, `login=3`.

Nhánh R+ (LỖI THỜI ở v2): trước đây có executable riêng `tencent_refresh` đọc
`refresh_session.json`. Bản v2 đã bỏ cả executable lẫn module `refresh_session`;
việc làm mới phiên do `AccountService::refresh` đảm nhiệm, lấy credential trực
tiếp từ account bundle. Kết quả verify cũ (offline framed C++ == Python byte-exact
trên session thật 1945B; live C++ ≡ Python) vẫn áp dụng vì đường dựng khung
không đổi.

Cập nhật mục này mỗi khi hoàn thành một phần việc (kèm test đã pass).
