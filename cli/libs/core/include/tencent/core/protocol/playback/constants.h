// getvinfo_constants.h — Bảng hằng số tĩnh (Group A) cho getvinfo.
//
// Port 1:1 từ ../analytics/app/getvinfo_constants.py (validated 2026-05-26 vs TencentVideo
// 9.02.90.31109). Đây là phần DUMP-một-lần (extract bằng IDA/strings), KHÔNG phải
// thuật toán — xem AGENTS.md §3 và ../analytics/app/plan_getvinfo_params.md §3 Group A.
//
// Mỗi bảng GIỮ THỨ TỰ như Python dict (body order FIXED).
#pragma once

#include <string>
#include <vector>

#include "tencent/core/common/ordered_kv.h"

namespace tencent::playback {
namespace constants {

// Build/session-independent constants used across the C++ port. Values are
// dumped from TencentVideo 9.02.90.31109 / libqimei.so and mirrored in the
// Python ground truth.
inline constexpr const char* kAppVersion = "9.02.90.31109";
inline constexpr const char* kGetvinfoUrl = "https://vv6.video.qq.com/getvinfo";
inline constexpr const char* kCkeyXorKeyHex = "842eed08f066e6ea48b4caa991ed6ff3";
inline constexpr const char* kCkeyTeaKeyHex = "7947a1d19371b533d9f6bbc83de4c253";
inline constexpr const char* kCkeyPreambleHex =
    "0000421500000015000011d70000283f0000000000000000";
inline constexpr uint32_t kCkeyRawBitmap = 0x00000007u;
inline constexpr const char* kCkeyTrailingHex =
    "0b382e312e322e3136302e340000000000000006";

inline constexpr const char* kCkeyField1Magic = "fceg";
inline constexpr const char* kQqlivePackageName = "com.tencent.qqlive";
inline constexpr const char* kQqliveCertHash = "4DE1C73B";
inline constexpr const char* kCkeyNullText = "null";
inline constexpr const char* kCkeyRuntimeVersion = "8.1.2.160.4";
inline constexpr const char* kCkeyDeviceSdkVersion = "6.22";
inline constexpr const char* kField15Ssk = "khym553sh0";

inline constexpr const char* kField16Bat = "x7kpm9wl3v";
inline constexpr const char* kField16AlgoId = "74zxj6xd79";
inline constexpr const char* kField16Cste = "8qnchzrchl";
inline constexpr const char* kField16Lv = "v4.0";
inline constexpr const char* kField16Csal0 = "m5h0zchrh5";
inline constexpr const char* kField16Csal1 = "9x7k6uc7xw";
inline constexpr const char* kField16Csal2 = "pplro0qj0r";
inline constexpr const char* kField16Csal3 = "k7tslimq88";
inline constexpr const char* kField16Csal4 = "o4stei6utn";

// CONSTANT_CAPS — output các native capability writer (getvinfo_constants.py:67-108).
// Nguồn writer: libtvkplayer.so tvk_vinfo_add_*_caps (xem getvinfo_capability_bits.md).
OrderedKV capability_bits();

// FORMAT_HINTS — cờ encoding/format theo build (getvinfo_constants.py:114-120).
OrderedKV format_hints();

// PLAYER_CTRL_DEFAULTS — default từ TVKPlayerVideoInfo.addExtraRequestParamsMap
// (getvinfo_constants.py:129-145). Gồm cả drm/defn/defnsrc/defnswitch.
OrderedKV player_ctrl_defaults();

// DEVICE_FIXED — request slots for Build.* + TVKPlatformInfo. Per-device
// values are placeholders and must be supplied by the account fingerprint.
OrderedKV device_fixed();

// EMPTY_DEFAULTS — field luôn rỗng trong playback (getvinfo_constants.py:224-227).
OrderedKV empty_defaults();

// SAFE_STATIC_CONSTANTS — gộp 5 bảng trên ĐÚNG THỨ TỰ
// (getvinfo_constants.py:245-251): CAPABILITY_BITS + FORMAT_HINTS +
// PLAYER_CTRL_DEFAULTS + DEVICE_FIXED + EMPTY_DEFAULTS.
OrderedKV safe_static_constants();

// Canonical getvinfo request layout. Values that depend on account/device or
// one request are placeholders and are overwritten by the runtime builder.
OrderedKV request_param_defaults();

// PER_REQUEST_RUNTIME (getvinfo_constants.py:170-190). value nullopt = "lấy từ
// template nếu có" (bitrate/newnettype/ipstack/atime); value có = set trực tiếp
// (mediascene/reportparams). KHÔNG hardcode các None field.
OptionalKV per_request_runtime();

// PATH_DEPENDENT (getvinfo_constants.py:211-218).
OrderedKV normal_path_additions();
OrderedKV history_path_required();

// get_path_additions (getvinfo_constants.py:260-267). mode = "normal" | "history".
OrderedKV path_additions(const std::string& mode);

// PER_REQUEST_GENERATED_KEYS (getvinfo_constants.py:235-239) — chỉ tham chiếu.
std::vector<std::string> per_request_generated_keys();

}  // namespace constants
}  // namespace tencent::playback
