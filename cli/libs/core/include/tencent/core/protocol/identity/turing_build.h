// turing_build.h — Turing K-value builders over the `turing-raw/1` device JSON.
//
// The APK only reads raw facts (see analytics/app/android_helpers/turingprobe/SCHEMA.md).
// This module owns every turing algorithm (murmur, simhash, K formatting, the
// /system DFS) and turns the raw snapshot into the 53 first-request K-values.
// Other request types (qimei, getvinfo) reuse the same raw JSON via their own
// builders — this one is turing-only.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "tencent/core/common/json.h"
#include "tencent/core/model/turing_material.h"

namespace tencent::turing {

// MurmurHash64A (libturingbase sub_14D58), seed 0 by default. Unsigned result;
// native prints it signed via %ld (use as int64 when formatting K125).
uint64_t murmur64a(const void* data, std::size_t len, uint64_t seed = 0);
inline uint64_t murmur64a(const std::string& s, uint64_t seed = 0) {
  return murmur64a(s.data(), s.size(), seed);
}

// Bit-vote a list of 64-bit hashes (sub_14F78).
uint64_t simhash64(const std::vector<uint64_t>& hashes);

// Fresh device-UUID (K26) — 32 hex, native UUID.randomUUID() fallback. The
// non-root probe can't read QQLive's private cache; reuse this for a stable id.
std::string synth_k26();

// Build all first-request K-values from the raw device snapshot.
// `raw` is the parsed `device_raw.json`. Missing/unsupported K's are "".
std::map<int, std::string> build_k_values(
    const nlohmann::json& raw, const Material* material = nullptr);

}  // namespace tencent::turing
