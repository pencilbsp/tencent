// capability.h — Lớp toggle cờ capability (codec/DRM/HDR/HLS) cho getvinfo.
//
// Port 1:1 từ ../analytics/app/capability_flags.py. Bit semantics: plan §14 +
// ../analytics/app/getvinfo_capability_bits.md + writer tvk_vinfo_add_*_caps trong
// libtvkplayer.so (đã reverse).
//
// "Quality thiết bị HỖ TRỢ" (khác với defn = quality NGƯỜI DÙNG CHỌN, ở Pha 4).
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "tencent/core/protocol/playback/request_context.h"
#include "tencent/core/common/ordered_kv.h"

namespace tencent::playback {

// ── Bit registries (capability_flags.py:32-84) ───────────────────────────────
namespace capbits {
// DRM_BITS
constexpr int64_t DRM_CHACHA20 = 0x008;
constexpr int64_t DRM_WIDEVINE = 0x020;
constexpr int64_t DRM_CHINADRM = 0x080;  // ChinaDRM 2.0
constexpr int64_t DRM_TXV = 0x400;       // Tencent DRM
// SPVIDEO_BITS
constexpr int64_t SPVIDEO_HDR10 = 0x0004;
constexpr int64_t SPVIDEO_HDR_ENHANCE = 0x0080;
constexpr int64_t SPVIDEO_HDR_VIVID = 0x0400;
constexpr int64_t SPVIDEO_VVC = 0x1000;  // H.266
// SPHLS_BITS
constexpr int64_t SPHLS_BASIC = 0x2;
constexpr int64_t SPHLS_PLUGIN_AD = 0x4;
constexpr int64_t SPHLS_EXTENDED_TAGS = 0x8;
}  // namespace capbits

// DOLBYDECODE_VARIANTS (capability_flags.py:93-98): tra giá trị theo tên.
// SPAUDIO_DOLBY_MASK là vùng bit Dolby riêng của spaudio.
int64_t dolby_variant_value(const std::string& name);
int64_t dolby_audio_mask();
// SPSFRHDR_TIERS (capability_flags.py:78-84).
int64_t spsfrhdr_tier_value(const std::string& name);

// to_param_dict trả về cặp (key, optional value): nullopt = "xóa key khỏi body"
// (capability_flags.py:247-267, None -> remove). = OptionalKV.
using ParamDict = OptionalKV;

// Snapshot toàn bộ cờ capability. Port của capability_flags.py:99-287.
// Mọi toggle TRẢ VỀ BẢN SAO đã sửa (immutable kiểu Python dataclass.replace).
struct CapabilitySet {
  int64_t drm = 1032;          // bit3+bit10 (chacha20+txv)
  int64_t sptxvdrm = 63;       // bits 0-5
  int64_t spvideo = 5124;      // bit2+bit10+bit12
  int64_t dolbydecode = 40;    // 0x28 atmos_combined
  int64_t sphls = 14;          // bits 1+2+3
  int64_t spm3u8tag = 19027;   // 7/8 HLS tag bits
  int64_t hevclv = 33;         // HEVC level 3.3
  int64_t spavs3 = 15;         // AVS3 tier 15
  int64_t sphevcfps = 70368744177663LL;
  int64_t spvvcfps = 47;
  int64_t spsfrhdr = 300;      // 60fps HDR10
  bool has_spuhdsfrhdr = true; // false = key vắng khỏi body (None)
  int64_t spuhdsfrhdr = 200;   // binary signal
  int64_t spaudio = 3471;
  int64_t spaudioeffectver = 5;
  int64_t spau = 284;
  OrderedKV spavg_extra;       // caller-set extras (capability_flags.py:121)

  // Read captured capability values from the assembled request context;
  // thiếu/empty -> default.
  static CapabilitySet from_context(const GetvinfoContext& context);

  // ── Toggles (capability_flags.py:154-215) — std::nullopt = không đổi ──────
  CapabilitySet with_drm(std::optional<bool> chacha20, std::optional<bool> widevine,
                         std::optional<bool> chinadrm, std::optional<bool> txv) const;
  CapabilitySet with_video(std::optional<bool> hdr10, std::optional<bool> hdr_enhance,
                           std::optional<bool> hdr_vivid, std::optional<bool> vvc) const;
  CapabilitySet with_hls(std::optional<bool> basic, std::optional<bool> plugin_ad,
                         std::optional<bool> extended_tags) const;
  CapabilitySet with_dolby(std::optional<std::string> variant) const;
  CapabilitySet with_hdr10_fps(std::optional<std::string> tier) const;
  CapabilitySet with_uhd_hdr_vivid(std::optional<bool> enabled) const;
  CapabilitySet with_max_hevc_level(std::optional<int64_t> level) const;
  CapabilitySet with_max_avs3_tier(std::optional<int64_t> tier) const;

  // ── Bundles (capability_flags.py:219-243) ────────────────────────────────
  CapabilitySet optional_features_off() const;
  CapabilitySet no_drm() const;
  CapabilitySet basic_only() const;
  CapabilitySet uhd_only_no_vvc() const;

  // ── Serialization (capability_flags.py:247-267) ──────────────────────────
  ParamDict to_param_dict() const;
};

// Tiện ích test/debug: serialize ParamDict/OrderedKV thành chuỗi tất định
// "key=value\n..." (value None -> "<NULL>"). Dùng so byte-exact (AGENTS.md §0).
std::string serialize_params(const ParamDict& params);
std::string serialize_params(const OrderedKV& params);

// ── Lựa chọn của NGƯỜI DÙNG (bật/tắt tính năng) ─────────────────────────────
//
// Mặc định mọi tính năng tuỳ chọn đều TẮT: yêu cầu càng ít khả năng thì server
// càng ít khả năng trả về luồng mà máy không giải mã được. Người dùng bật thêm
// từng thứ khi cần.
struct CapabilityOptions {
  bool enable_vvc = false;          // H.266
  bool enable_hdr = false;          // HDR10
  bool enable_hdr_enhance = false;
  bool enable_hdr_vivid = false;
  bool enable_drm = false;          // giữ nguyên bitmask DRM của thiết bị
  std::string dolby;                // tên biến thể Dolby ("" = tắt)
  std::string hdr_fps;              // bậc HDR10 theo fps ("" = tắt)
};

// Áp lựa chọn người dùng lên khả năng THIẾT BỊ đọc từ `context`, rồi trả về
// tập tham số ghi đè cho body getvinfo (nullopt = xoá key).
// Ném std::invalid_argument nếu dolby/hdr_fps là "none".
ParamDict build_capability_overrides(const GetvinfoContext& context,
                                     const CapabilityOptions& options);

}  // namespace tencent::playback
