// test_capability.cpp — Test cho getvinfo_constants + CapabilitySet (Pha 3).
//
// So serialize_params(...) với ground-truth Python (capability_flags /
// getvinfo_constants) trong generated_vectors.h. Xem AGENTS.md §0.
#include <cstdio>
#include <optional>
#include <string>

#include "tencent/core/protocol/playback/capability.h"
#include "vectors/generated_vectors.h"
#include "tencent/core/protocol/playback/request_context.h"
#include "tencent/core/protocol/playback/constants.h"

using tencent::playback::CapabilitySet;
using tencent::playback::serialize_params;
namespace C = tencent::playback::constants;

static int g_failures = 0;
static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}
static void check_eq(const std::string& got, const char* want, const char* name) {
  bool ok = got == std::string(want);
  check(ok, name);
  if (!ok) {
    std::printf("    want: %s\n", want);
    std::printf("    got : %s\n", got.c_str());
  }
}

int main() {
  std::printf("== Pha 3: capability + constants ==\n");
  using opt_b = std::optional<bool>;
  const opt_b N = std::nullopt;  // "không đổi"

  CapabilitySet def;  // mặc định

  // ── CapabilitySet ───────────────────────────────────────────────────────
  check_eq(serialize_params(def.to_param_dict()), vec::CAPS_DEFAULT, "caps default");

  CapabilitySet off = def.optional_features_off();
  check(off.drm == 0 && off.sptxvdrm == 0, "optional baseline disables DRM");
  check(off.spvideo == 0 && off.spvvcfps == 0,
        "optional baseline disables HDR/VVC video bits");
  check(off.dolbydecode == 0, "optional baseline disables Dolby");
  check(off.spsfrhdr == 0 && !off.has_spuhdsfrhdr,
        "optional baseline disables HDR tiers");

  tencent::playback::GetvinfoContext context;
  context.query_params = {
      {"spvideo", "5124"}, {"drm", "1032"}, {"hevclv", "51"},
      {"spvvcfps", "31"},  {"spuhdsfrhdr", "200"},
  };
  check_eq(serialize_params(CapabilitySet::from_context(context).to_param_dict()),
           vec::CAPS_FROM_TPL, "caps from_context (đọc hevclv=51, spvvcfps=31...)");

  check_eq(serialize_params(def.with_video(/*hdr10=*/true, /*hdr_enhance=*/N,
                                           /*hdr_vivid=*/N, /*vvc=*/false)
                                .with_dolby("profile_a")
                                .to_param_dict()),
           vec::CAPS_VIDEO, "caps with_video(vvc off,hdr10 on)+dolby(profile_a)");

  check_eq(serialize_params(def.basic_only().to_param_dict()), vec::CAPS_BASIC,
           "caps basic_only (spuhdsfrhdr -> NULL)");

  check_eq(serialize_params(def.uhd_only_no_vvc().to_param_dict()), vec::CAPS_UHD_NO_VVC,
           "caps uhd_only_no_vvc");

  check_eq(serialize_params(def.no_drm().to_param_dict()), vec::CAPS_NO_DRM, "caps no_drm");

  check_eq(serialize_params(def.with_hdr10_fps("50fps").with_uhd_hdr_vivid(false).to_param_dict()),
           vec::CAPS_HDR_UHDOFF, "caps hdr10_fps(50)+uhd off (NULL)");

  // ── Hằng số (getvinfo_constants) ────────────────────────────────────────
  std::string static_expected = vec::CONST_SAFE_STATIC;
  std::string captured_model = "devicemodel=SM-G991N\n";
  std::size_t captured_model_pos = static_expected.find(captured_model);
  if (captured_model_pos != std::string::npos)
    static_expected.replace(captured_model_pos, captured_model.size(), "devicemodel=\n");
  check_eq(serialize_params(C::safe_static_constants()), static_expected.c_str(),
           "SAFE_STATIC_CONSTANTS leaves device model empty");
  check_eq(serialize_params(C::path_additions("normal")), vec::CONST_PATH_NORMAL,
           "path_additions(normal)");
  check_eq(serialize_params(C::path_additions("history")), vec::CONST_PATH_HISTORY,
           "path_additions(history)");
  check_eq(C::kAppVersion, vec::TPL_APP_VER, "kAppVersion");
  check_eq(C::kGetvinfoUrl, vec::TPL_REQUEST_URL, "kGetvinfoUrl");
  check_eq(C::kCkeyXorKeyHex, vec::TPL_XOR_KEY_HEX, "kCkeyXorKeyHex");
  check_eq(C::kCkeyTeaKeyHex, vec::TPL_TEA_KEY_HEX, "kCkeyTeaKeyHex");

  // ── Negative: mode lạ / variant lạ ném lỗi ──────────────────────────────
  bool threw = false;
  try { C::path_additions("bogus"); } catch (const std::exception&) { threw = true; }
  check(threw, "path_additions(mode lạ) ném lỗi");

  threw = false;
  try { def.with_dolby("bogus"); } catch (const std::exception&) { threw = true; }
  check(threw, "with_dolby(variant lạ) ném lỗi");

  // ── DOLBY_AUDIO_MASK = 0x3E ─────────────────────────────────────────────
  check(tencent::playback::dolby_audio_mask() == 0x3E, "dolby_audio_mask == 0x3E");

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
