// ckey_fields.h — Builders for solved dynamic cKey v4.2 TLV fields.
//
// Sources:
//   - Field[15]/[16]: ../analytics/app/encode_ckey_v42.py
//   - Field[19]: ../analytics/app/field19_session_port.py
//   - Analysis: ../analytics/app/ckey_v42_analysis.md §§15, 19-28.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "tencent/core/common/bytes.h"

namespace tencent::playback {

// Field[15] playback/native JSON. Port of encode_ckey_v42.py::build_field15_json.
Bytes build_field15_json_native();

// Canonical cKey v4.2 TLV base: device/build fields are passed in, dynamic
// fields are placeholders overwritten by generate_ckey.
std::vector<Bytes> build_ckey_base_fields(const std::string& app_ver,
                                          const std::string& guid,
                                          const std::string& qimei36,
                                          const std::string& device_model);

// Field[16].dd native formula. Port of encode_ckey_v42.py::build_field16_dd.
std::string build_field16_dd(uint64_t timestamp_ms, const Bytes& random8);

// Field[16] canonical crypto-algo JSON. Port of
// encode_ckey_v42.py::build_field16_crypto_algo_json.
Bytes build_field16_crypto_algo_json(const std::string& sd, const std::string& dd);

// Field[19] full-chain native port. `component2` is the selected video/asset id
// passed to libqimei `.c_g_ck` arg3; in current getvinfo it is vid-like and not
// necessarily hexadecimal.
Bytes derive_field19_from_components(uint64_t epoch_seconds, const std::string& component2,
                                     const std::string& qimei36, uint32_t micros);

}  // namespace tencent::playback
