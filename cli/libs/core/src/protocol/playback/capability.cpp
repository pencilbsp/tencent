// capability.cpp — Xem capability.h. Port từ ../analytics/app/capability_flags.py.
#include "tencent/core/protocol/playback/capability.h"

#include <map>
#include <stdexcept>

namespace tencent::playback {

// ── Registry tra theo tên ────────────────────────────────────────────────────

// DOLBYDECODE_VARIANTS (capability_flags.py:93-98).
static const std::map<std::string, int64_t>& dolby_variants() {
  static const std::map<std::string, int64_t> m = {
      {"none", 0x00}, {"profile_a", 0x14}, {"profile_b", 0x28}, {"both", 0x3C},
  };
  return m;
}

int64_t dolby_variant_value(const std::string& name) {
  auto it = dolby_variants().find(name);
  if (it == dolby_variants().end())
    throw std::invalid_argument("unknown dolby variant: " + name);
  return it->second;
}

// SPAUDIO_DOLBY_MASK = bits 1-5 (capability_flags.py:67-68).
int64_t dolby_audio_mask() { return 0x3E; }

// SPSFRHDR_TIERS (capability_flags.py:78-84).
int64_t spsfrhdr_tier_value(const std::string& name) {
  static const std::map<std::string, int64_t> m = {
      {"60fps", 300}, {"50fps", 250}, {"30fps", 200}, {"24fps", 100}, {"none", 0},
  };
  auto it = m.find(name);
  if (it == m.end()) throw std::invalid_argument("unknown HDR10 fps tier: " + name);
  return it->second;
}

// _flip_bit (capability_flags.py:87-93). nullopt -> giữ nguyên.
static int64_t flip_bit(int64_t value, int64_t bit, std::optional<bool> enable) {
  if (!enable.has_value()) return value;
  return *enable ? (value | bit) : (value & ~bit);
}

// ── Context overlay (capability_flags.py:125-150) ────────────────────────────

CapabilitySet CapabilitySet::from_context(const GetvinfoContext& context) {
  auto get_int = [&context](const std::string& key, int64_t def) -> int64_t {
    const std::string* v = context.find_param(key);
    if (v == nullptr || v->empty()) return def;
    return std::stoll(*v);
  };

  CapabilitySet c;
  c.drm = get_int("drm", 1032);
  c.sptxvdrm = get_int("sptxvdrm", 63);
  c.spvideo = get_int("spvideo", 5124);
  c.dolbydecode = get_int("dolbydecode", 40);
  c.sphls = get_int("sphls", 14);
  c.spm3u8tag = get_int("spm3u8tag", 19027);
  c.hevclv = get_int("hevclv", 33);
  c.spavs3 = get_int("spavs3", 15);
  c.sphevcfps = get_int("sphevcfps", 70368744177663LL);
  c.spvvcfps = get_int("spvvcfps", 47);
  c.spsfrhdr = get_int("spsfrhdr", 300);
  // spuhdsfrhdr: present -> int; absent/empty -> None (has=false).
  const std::string* uhd = context.find_param("spuhdsfrhdr");
  if (uhd != nullptr && !uhd->empty()) {
    c.has_spuhdsfrhdr = true;
    c.spuhdsfrhdr = std::stoll(*uhd);
  } else {
    c.has_spuhdsfrhdr = false;
  }
  c.spaudio = get_int("spaudio", 3471);
  c.spaudioeffectver = get_int("spaudioeffectver", 5);
  c.spau = get_int("spau", 284);
  return c;
}

// ── Toggles ──────────────────────────────────────────────────────────────────

CapabilitySet CapabilitySet::with_drm(std::optional<bool> chacha20, std::optional<bool> widevine,
                                      std::optional<bool> chinadrm,
                                      std::optional<bool> txv) const {
  CapabilitySet c = *this;
  c.drm = flip_bit(c.drm, capbits::DRM_CHACHA20, chacha20);
  c.drm = flip_bit(c.drm, capbits::DRM_WIDEVINE, widevine);
  c.drm = flip_bit(c.drm, capbits::DRM_CHINADRM, chinadrm);
  c.drm = flip_bit(c.drm, capbits::DRM_TXV, txv);
  return c;
}

CapabilitySet CapabilitySet::with_video(std::optional<bool> hdr10, std::optional<bool> hdr_enhance,
                                        std::optional<bool> hdr_vivid,
                                        std::optional<bool> vvc) const {
  CapabilitySet c = *this;
  c.spvideo = flip_bit(c.spvideo, capbits::SPVIDEO_HDR10, hdr10);
  c.spvideo = flip_bit(c.spvideo, capbits::SPVIDEO_HDR_ENHANCE, hdr_enhance);
  c.spvideo = flip_bit(c.spvideo, capbits::SPVIDEO_HDR_VIVID, hdr_vivid);
  c.spvideo = flip_bit(c.spvideo, capbits::SPVIDEO_VVC, vvc);
  return c;
}

CapabilitySet CapabilitySet::with_hls(std::optional<bool> basic, std::optional<bool> plugin_ad,
                                      std::optional<bool> extended_tags) const {
  CapabilitySet c = *this;
  c.sphls = flip_bit(c.sphls, capbits::SPHLS_BASIC, basic);
  c.sphls = flip_bit(c.sphls, capbits::SPHLS_PLUGIN_AD, plugin_ad);
  c.sphls = flip_bit(c.sphls, capbits::SPHLS_EXTENDED_TAGS, extended_tags);
  return c;
}

// capability_flags.py:239-255
CapabilitySet CapabilitySet::with_dolby(std::optional<std::string> variant) const {
  if (!variant.has_value()) return *this;
  int64_t val = dolby_variant_value(*variant);
  CapabilitySet c = *this;
  c.dolbydecode = val;
  c.spaudio &= ~dolby_audio_mask();
  if (val != 0) c.spaudio |= 0x0E;
  return c;
}

// capability_flags.py:197-203
CapabilitySet CapabilitySet::with_hdr10_fps(std::optional<std::string> tier) const {
  if (!tier.has_value()) return *this;
  CapabilitySet c = *this;
  c.spsfrhdr = spsfrhdr_tier_value(*tier);
  return c;
}

// capability_flags.py:205-209
CapabilitySet CapabilitySet::with_uhd_hdr_vivid(std::optional<bool> enabled) const {
  if (!enabled.has_value()) return *this;
  CapabilitySet c = *this;
  if (*enabled) {
    c.has_spuhdsfrhdr = true;
    c.spuhdsfrhdr = 200;
  } else {
    c.has_spuhdsfrhdr = false;
  }
  return c;
}

CapabilitySet CapabilitySet::with_max_hevc_level(std::optional<int64_t> level) const {
  if (!level.has_value()) return *this;
  CapabilitySet c = *this;
  c.hevclv = *level;
  return c;
}

CapabilitySet CapabilitySet::with_max_avs3_tier(std::optional<int64_t> tier) const {
  if (!tier.has_value()) return *this;
  CapabilitySet c = *this;
  c.spavs3 = *tier;
  return c;
}

// ── Bundles ──────────────────────────────────────────────────────────────────

CapabilitySet CapabilitySet::optional_features_off() const {
  CapabilitySet c = no_drm();
  c = c.with_video(/*hdr10=*/false, /*hdr_enhance=*/false,
                   /*hdr_vivid=*/false, /*vvc=*/false);
  c = c.with_dolby(std::string("none"));
  c = c.with_uhd_hdr_vivid(false);
  c.spvvcfps = 0;
  c.spsfrhdr = 0;
  return c;
}

// capability_flags.py:219-221
CapabilitySet CapabilitySet::no_drm() const {
  CapabilitySet c = *this;
  c.drm = 0;
  c.sptxvdrm = 0;
  return c;
}

// capability_flags.py:223-236
CapabilitySet CapabilitySet::basic_only() const {
  CapabilitySet c = *this;
  c.drm = capbits::DRM_CHACHA20;  // giữ chacha20
  c.sptxvdrm = 0;
  c.spvideo = 0;
  c.spvvcfps = 0;
  c.spsfrhdr = 0;
  c.has_spuhdsfrhdr = false;  // None
  c.dolbydecode = 0;
  c.spaudio = 1;
  c.spavs3 = 0;
  return c;
}

// capability_flags.py:238-243
CapabilitySet CapabilitySet::uhd_only_no_vvc() const {
  CapabilitySet c = with_video(/*hdr10=*/true, /*hdr_enhance=*/std::nullopt,
                               /*hdr_vivid=*/true, /*vvc=*/false);
  c.spvvcfps = 0;
  return c;
}

// ── Serialization (capability_flags.py:247-267) ──────────────────────────────

ParamDict CapabilitySet::to_param_dict() const {
  ParamDict out = {
      {"drm", std::to_string(drm)},
      {"sptxvdrm", std::to_string(sptxvdrm)},
      {"spvideo", std::to_string(spvideo)},
      {"dolbydecode", std::to_string(dolbydecode)},
      {"sphls", std::to_string(sphls)},
      {"spm3u8tag", std::to_string(spm3u8tag)},
      {"hevclv", std::to_string(hevclv)},
      {"spavs3", std::to_string(spavs3)},
      {"sphevcfps", std::to_string(sphevcfps)},
      {"spvvcfps", std::to_string(spvvcfps)},
      {"spsfrhdr", std::to_string(spsfrhdr)},
      {"spaudio", std::to_string(spaudio)},
      {"spaudioeffectver", std::to_string(spaudioeffectver)},
      {"spau", std::to_string(spau)},
  };
  // spuhdsfrhdr: value hoặc None (capability_flags.py:265).
  out.emplace_back("spuhdsfrhdr", has_spuhdsfrhdr
                                      ? std::optional<std::string>(std::to_string(spuhdsfrhdr))
                                      : std::nullopt);
  // spavg_extra.update (capability_flags.py:266).
  for (const auto& kv : spavg_extra) out.emplace_back(kv.first, kv.second);
  return out;
}

// ── Serialize helpers ────────────────────────────────────────────────────────

std::string serialize_params(const ParamDict& params) {
  std::string out;
  for (const auto& kv : params) {
    out += kv.first;
    out += '=';
    out += kv.second.has_value() ? *kv.second : std::string("<NULL>");
    out += '\n';
  }
  return out;
}

std::string serialize_params(const OrderedKV& params) {
  std::string out;
  for (const auto& kv : params) {
    out += kv.first;
    out += '=';
    out += kv.second;
    out += '\n';
  }
  return out;
}

// ── Lựa chọn người dùng → tham số ghi đè ────────────────────────────────────
// Chuyển từ build_capability_overrides() trong tencent_cli_main.cpp cũ.

ParamDict build_capability_overrides(const GetvinfoContext& context,
                                     const CapabilityOptions& options) {
  // Khả năng THIẾT BỊ (đọc từ ngữ cảnh) là trần trên; mặc định tắt hết tuỳ chọn.
  const CapabilitySet device = CapabilitySet::from_context(context);
  CapabilitySet caps = device.optional_features_off();
  int64_t vvc_fps_mask = device.spvvcfps;

  if (options.enable_drm) {
    caps.drm = device.drm;
    caps.sptxvdrm = device.sptxvdrm;
  }
  if (options.enable_vvc) {
    // Một số bản dựng khai báo mặt nạ fps của VVC thấp hơn HEVC; lấy giá trị
    // lớn hơn để không tự giới hạn khung hình.
    vvc_fps_mask = device.spvvcfps < device.sphevcfps ? device.sphevcfps : device.spvvcfps;
    caps.spvideo =
        device.with_video(std::nullopt, std::nullopt, std::nullopt, true).spvideo;
    caps.spvvcfps = vvc_fps_mask;
  }
  if (options.enable_hdr || !options.hdr_fps.empty()) {
    caps = caps.with_video(true, std::nullopt, std::nullopt, std::nullopt);
    caps.spsfrhdr = device.spsfrhdr;
  }
  if (options.enable_hdr_enhance) {
    caps = caps.with_video(std::nullopt, true, std::nullopt, std::nullopt);
  }
  if (options.enable_hdr_vivid) {
    caps = caps.with_video(std::nullopt, std::nullopt, true, std::nullopt);
    caps = caps.with_uhd_hdr_vivid(true);
  }
  if (!options.dolby.empty()) {
    if (options.dolby == "none") {
      throw std::invalid_argument("giá trị Dolby không hợp lệ: 'none'");
    }
    caps = caps.with_dolby(options.dolby);
  }
  if (!options.hdr_fps.empty()) {
    if (options.hdr_fps == "none") {
      throw std::invalid_argument("giá trị HDR fps không hợp lệ: 'none'");
    }
    caps = caps.with_hdr10_fps(options.hdr_fps);
  }

  ParamDict overrides = caps.to_param_dict();
  if (options.enable_vvc) {
    // Native gửi kèm mặt nạ gốc trong reportparams khi bật VVC.
    set_value(overrides, "reportparams",
              "orispvvcfps=" + std::to_string(vvc_fps_mask) +
                  "&abtestid=&fwexpid=&spvvcfpsreasonnew=0&fwtype=2");
  }
  return overrides;
}

}  // namespace tencent::playback
