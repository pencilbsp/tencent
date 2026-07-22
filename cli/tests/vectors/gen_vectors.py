#!/usr/bin/env python3
"""
gen_vectors.py — NGUỒN SỰ THẬT DUY NHẤT cho test byte-exact của cli/ (AGENTS.md §1).

Nó import trực tiếp implementation Python gốc trong ../analytics/app/, nạp dữ liệu CAPTURE
thật (session_template.json), patch os.urandom về tất định, rồi xuất ra
tests/generated_vectors.h (committed). Test C++ so khớp BYTE-FOR-BYTE với header này.

Chạy:  python3 tests/gen_vectors.py
Sau đó: cmake --build build && ctest --test-dir build --output-on-failure
"""
from __future__ import annotations

import os
import re
import struct
import sys
import urllib.parse
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent   # tests/
APP_DIR = HERE.parent.parent / "analytics" / "app"
TEMPLATE_PATH = Path.home() / ".tencent" / "session_template.json"
# cKey capture thật, đã pass live (em=0/exem=0) — dùng cho vector DECODE/roundtrip.
REQUEST_PATH = APP_DIR / "request.authdump.txt"
# Fixture synthetic (không secret) để test loader — committed, tự chứa.
FIXTURE_PATH = HERE / "fixtures" / "session_template.sample.json"

# ── Patch os.urandom -> tất định, có ghi log byte đã tiêu thụ ─────────────────
# Trong generate_ckey mới:
#   - Field[16].dd gọi os.urandom(8)
#   - encode_ckey/qqtea_pad_and_encrypt gọi os.urandom(2), rồi os.urandom(1)
_urandom_log: list[bytes] = []
_urandom_pos = {"v": 0}


def _fake_urandom(n: int) -> bytes:
    start = _urandom_pos["v"]
    out = bytes((start + i) & 0xFF for i in range(n))
    _urandom_pos["v"] = start + n
    _urandom_log.append(out)
    return out


os.urandom = _fake_urandom  # patch TRƯỚC khi import module dùng nó

sys.path.insert(0, str(APP_DIR))
from parse_ckey_v42 import (  # noqa: E402
    poly131, tencent_b64_encode, tencent_b64_decode, xor_bytes,
    qqtea_cbc_encrypt, qqtea_cbc_decrypt, _tea_enc_block, _tea_dec_block,
)
from encode_ckey_v42 import encode_ckey, parse_full_captured  # noqa: E402
from session_template import SessionTemplate  # noqa: E402
import capability_flags as caps  # noqa: E402
import getvinfo_params as gp  # noqa: E402
from getvinfo_constants import (  # noqa: E402
    SAFE_STATIC_CONSTANTS, get_path_additions,
)

CKEY_FIXTURE_PATH = HERE / "fixtures" / "session_template.ckey.json"


def h(b: bytes) -> str:
    return b.hex()


def main() -> int:
    if not TEMPLATE_PATH.exists():
        print(f"[FATAL] không tìm thấy template capture: {TEMPLATE_PATH}", file=sys.stderr)
        return 1

    # ── Primitive vectors (tất định, không cần random) ───────────────────────
    poly_in = bytes(range(64))
    poly_out = poly131(poly_in)

    b64_in = bytes(range(50))  # 50 % 3 == 2 -> kiểm tra nhánh padding rem==2
    b64_str = tencent_b64_encode(b64_in)

    b64_in2 = bytes(range(49))  # 49 % 3 == 1 -> nhánh rem==1
    b64_str2 = tencent_b64_encode(b64_in2)

    xor_in = bytes(range(40))
    xor_key_sample = bytes.fromhex("842eed08f066e6ea48b4caa991ed6ff3")
    xor_out = xor_bytes(xor_in, xor_key_sample)

    tea_key_sample = bytes.fromhex("7947a1d19371b533d9f6bbc83de4c253")
    tea_block_in = bytes(range(8))
    tea_block_enc = _tea_enc_block(tea_block_in, tea_key_sample)
    tea_block_dec = _tea_dec_block(tea_block_in, tea_key_sample)

    cbc_in = bytes(range(32))  # bội số 8
    cbc_enc = qqtea_cbc_encrypt(cbc_in, tea_key_sample)
    cbc_dec = qqtea_cbc_decrypt(cbc_in, tea_key_sample)

    # ── cKey vector từ template CAPTURE thật ─────────────────────────────────
    t = SessionTemplate.load(TEMPLATE_PATH)
    fields = [bytes.fromhex(x) for x in t.tlv_fields_hex]
    preamble = t.preamble()
    tea_key = t.tea_key()
    xor_key = t.xor_key()
    raw_bitmap = t.raw_bitmap
    raw_ints = tuple(t.raw_ints)
    trailing = t.trailing()

    _urandom_log.clear()
    _urandom_pos["v"] = 0xA0  # seed cố định để rand2/head1 rõ ràng
    ckey = encode_ckey(fields, preamble, tea_key, xor_key, raw_bitmap, raw_ints, trailing)
    # encode_ckey -> qqtea_pad_and_encrypt: urandom(2) rồi urandom(1).
    assert len(_urandom_log) == 2, f"kỳ vọng 2 lần urandom, có {len(_urandom_log)}"
    rand2 = _urandom_log[0]
    head_rand1 = _urandom_log[1][0]

    # ── cKey DECODE vector từ capture thật (request.authdump.txt) ────────────
    if not REQUEST_PATH.exists():
        print(f"[FATAL] không tìm thấy capture: {REQUEST_PATH}", file=sys.stderr)
        return 1
    m = re.search(r"cKey=([^&'\s]+)", REQUEST_PATH.read_text())
    if not m:
        print(f"[FATAL] không thấy cKey trong {REQUEST_PATH}", file=sys.stderr)
        return 1
    cap_ckey = urllib.parse.unquote(m.group(1))

    # Giải mã bằng CHÍNH các khóa của template (session-independent).
    # LƯU Ý: cKey capture thật có vùng pad NGẪU NHIÊN (khác \x00), nên KHÔNG thể
    # encode-lại byte-exact bằng encoder của ta (encoder ghi \x00). decode vẫn
    # đúng vì nó bỏ qua vùng pad. Vì vậy:
    #   - vector DECODE: so từng trường của decode(CAP_CKEY) với parse_full_captured.
    #   - vector ROUNDTRIP: decode→encode trên CKEY_EXPECTED (do CHÍNH ta sinh, pad=0).
    d_pre, d_fields, d_bitmap, d_ints, d_trailing, d_meta = parse_full_captured(
        cap_ckey, xor_key, tea_key)
    assert d_meta["stored_cs"] == d_meta["computed_cs"], "checksum capture không khớp"

    # Sanity Python: decode(CKEY_EXPECTED) phải khôi phục đúng fields template, và
    # encode lại bằng entropy tái lập phải ra đúng CKEY_EXPECTED (byte-exact).
    rp_pre, rp_fields, rp_bm, rp_ints, rp_trail, _ = parse_full_captured(ckey, xor_key, tea_key)
    assert rp_pre == preamble and rp_fields == fields, "decode(CKEY_EXPECTED) sai fields"
    _plain = qqtea_cbc_decrypt(xor_bytes(tencent_b64_decode(ckey[4:]), xor_key)[:-4], tea_key)
    _seq = iter([_plain[1:3], bytes([_plain[0]])])
    os.urandom = lambda n: next(_seq)  # type: ignore
    assert encode_ckey(rp_fields, rp_pre, tea_key, xor_key, rp_bm, tuple(rp_ints),
                       rp_trail) == ckey, "roundtrip CKEY_EXPECTED không khớp"
    os.urandom = _fake_urandom  # khôi phục

    d_fields_hex = ",\n      ".join(f'"{h(f)}"' for f in d_fields)
    d_ints_cpp = ", ".join(str(v) for v in d_ints)

    # ── session_template loader vector (từ fixture synthetic) ────────────────
    st = SessionTemplate.load(FIXTURE_PATH)

    def cpp_str_list(items):
        return ",\n      ".join(f'"{x}"' for x in items)

    tpl_ints = ", ".join(str(v) for v in st.raw_ints)
    tpl_qkeys = cpp_str_list([k for k, _ in st.query_params.items()])
    tpl_qvals = cpp_str_list([v for _, v in st.query_params.items()])
    tpl_hkeys = cpp_str_list([k for k, _ in st.headers.items()])
    tpl_hvals = cpp_str_list([v for _, v in st.headers.items()])

    # ── capability + constants vectors (Pha 3) ───────────────────────────────
    def pyser_paramdict(d):
        return "".join(f"{k}={'<NULL>' if v is None else v}\n" for k, v in d.items())

    def pyser_ordered(d):
        return "".join(f"{k}={v}\n" for k, v in d.items())

    def cpp_lit(s):
        return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")

    _default = caps.CapabilitySet()
    caps_default = pyser_paramdict(_default.to_param_dict())
    caps_from_tpl = pyser_paramdict(caps.CapabilitySet.from_template(st).to_param_dict())
    caps_video = pyser_paramdict(
        _default.with_video(vvc=False, hdr10=True).with_dolby("profile_a").to_param_dict())
    caps_basic = pyser_paramdict(_default.basic_only().to_param_dict())
    caps_uhd_no_vvc = pyser_paramdict(_default.uhd_only_no_vvc().to_param_dict())
    caps_no_drm = pyser_paramdict(_default.no_drm().to_param_dict())
    caps_hdr_uhdoff = pyser_paramdict(
        _default.with_hdr10_fps("50fps").with_uhd_hdr_vivid(False).to_param_dict())

    const_safe = pyser_ordered(SAFE_STATIC_CONSTANTS)
    const_path_normal = pyser_ordered(get_path_additions("normal"))
    const_path_history = pyser_ordered(get_path_additions("history"))

    # ── Pha 4: getvinfo_params (flowid, encode_body, ckey, build) ─────────────
    # flowid: f"{uuid}{nano:014d}_{platform}" (getvinfo_params.py:91).
    flow_uuid = "12345678-1234-4234-8234-123456789abc"
    flow_nano = 46886803916039
    flow_platform = "10303"
    flowid_expected = f"{flow_uuid}{flow_nano:014d}_{flow_platform}"
    flow_nano2 = 42  # kiểm zero-pad 14 chữ số
    flowid_expected2 = f"{flow_uuid}{flow_nano2:014d}_{flow_platform}"

    # encode_body: chuỗi ký tự khó (space,=,&,*,/,:,unicode,~,%).
    encbody_items = [
        ("a", "hello world"), ("b", "x=1&y=2"), ("c", "1080*2176"),
        ("d", "a/b:c"), ("e", "café"), ("f", "~tilde"),
        ("g", "100%done"), ("h", ""),
    ]
    encbody_expected = gp.encode_body(dict(encbody_items))

    # generate_ckey tất định: inject fresh_param2_and_random + pad.
    ckey_tpl = SessionTemplate.load(CKEY_FIXTURE_PATH)
    def base_ckey_fields_hex(template: SessionTemplate) -> list[str]:
        def hx(s: str) -> str:
            return s.encode("ascii").hex()
        qp = template.query_params
        app_ver = qp.get("appVer") or template.app_ver
        guid = qp.get("guid") or template.guid
        qimei = qp.get("qimei") or template.qimei36
        model = qp.get("devicemodel") or "SM-G991N"
        fields = [
            hx("fceg"), hx(""), hx(app_ver), hx(""), hx(guid),
            hx("com.tencent.qqlive"), hx("com.tencent.qqlive"), hx("4DE1C73B"), hx("null"),
            hx("com.tencent.qqlive"), hx("4DE1C73B"), hx("null"), hx(""), hx("8.1.2.160.4"),
            gp.build_field15_json(path="native").hex(), hx("{}"), hx(qimei), hx(qimei),
            ("00" * 26), hx("0" * 29), hx("0" * 16), hx(app_ver),
            hx(""), hx(""), hx(model), hx("6.22"),
            hx(""), hx(""), hx(""), hx(""), hx(""), hx(""), hx(""), hx(""), hx(""), hx(""),
            hx(""), "00", hx(""), hx(""), hx(""),
        ]
        assert len(fields) == 41
        return fields
    ckey_tpl.tlv_fields_hex = base_ckey_fields_hex(ckey_tpl)
    inj_param2 = b"1780470351116" + b"a" * 16
    inj_rand21 = b"b" * 16
    inj_tvusec = 656810
    gp.fresh_param2_and_random = lambda: (inj_param2, inj_rand21, inj_tvusec)
    _urandom_log.clear()
    _urandom_pos["v"] = 0xC0
    ckey_gen = gp.generate_ckey(ckey_tpl, vid="TESTVID123", bind_vid=True)
    assert len(_urandom_log) == 3, f"ckey urandom calls = {len(_urandom_log)}"
    ckey_dd_random8 = _urandom_log[0]
    ckey_rand2 = _urandom_log[1]
    ckey_head1 = _urandom_log[2][0]

    # build_query_params: flowid+ckey cố định -> tất định.
    FIXED_FLOW = "FLOWFIXED123_10303"
    FIXED_CKEY = "--01CKEYFIXED"
    extra_norm = {"spvideo": "7777", "drm": None, "extraNew": "E1"}
    build_normal_params = gp.build_query_params(
        ckey_tpl, "NEWVID", cid="NEWCID", defn="uhd", scene="playerUiType=0",
        mode="normal", atime="111", bitrate="222", flowid=FIXED_FLOW, ckey=FIXED_CKEY,
        extra=extra_norm)
    build_normal = pyser_ordered(build_normal_params)
    build_normal_body = gp.encode_body(build_normal_params)

    build_history_params = gp.build_query_params(
        ckey_tpl, "NEWVID", defn="hd", mode="history", flowid=FIXED_FLOW, ckey=FIXED_CKEY,
        previd_json='{"h":"1"}', extra={})
    build_history = pyser_ordered(build_history_params)

    # ── Sinh header C++ ──────────────────────────────────────────────────────
    fields_hex = ",\n      ".join(f'"{h(f)}"' for f in fields)
    raw_ints_cpp = ", ".join(str(v) for v in raw_ints)

    out = f"""// AUTO-GENERATED bởi tests/gen_vectors.py — KHÔNG SỬA TAY.
// Nguồn ground-truth: ../analytics/app/parse_ckey_v42.py, ../analytics/app/encode_ckey_v42.py
// + capture thật {TEMPLATE_PATH}.
// Sinh lại: python3 tests/gen_vectors.py   (AGENTS.md §1)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vec {{

// poly131(bytes(range(64)))
inline const char* POLY131_IN_HEX = "{h(poly_in)}";
inline const uint32_t POLY131_OUT = 0x{poly_out:08x}u;

// tencent_b64_encode — kiểm tra cả hai nhánh padding (rem==2 và rem==1)
inline const char* B64_IN_HEX  = "{h(b64_in)}";
inline const char* B64_OUT     = "{b64_str}";
inline const char* B64_IN2_HEX = "{h(b64_in2)}";
inline const char* B64_OUT2    = "{b64_str2}";

// xor_bytes(bytes(range(40)), key)
inline const char* XOR_IN_HEX  = "{h(xor_in)}";
inline const char* XOR_KEY_HEX = "{h(xor_key_sample)}";
inline const char* XOR_OUT_HEX = "{h(xor_out)}";

// TEA block đơn (tea_key_sample, block=bytes(range(8)))
inline const char* TEA_KEY_HEX       = "{h(tea_key_sample)}";
inline const char* TEA_BLOCK_IN_HEX  = "{h(tea_block_in)}";
inline const char* TEA_BLOCK_ENC_HEX = "{h(tea_block_enc)}";
inline const char* TEA_BLOCK_DEC_HEX = "{h(tea_block_dec)}";

// QQ-TEA CBC dual-state (input=bytes(range(32)))
inline const char* CBC_IN_HEX  = "{h(cbc_in)}";
inline const char* CBC_ENC_HEX = "{h(cbc_enc)}";
inline const char* CBC_DEC_HEX = "{h(cbc_dec)}";

// cKey v4.2 đầy đủ — từ template CAPTURE thật, entropy tất định.
inline const char* CKEY_PREAMBLE_HEX = "{h(preamble)}";
inline const std::vector<std::string> CKEY_FIELDS_HEX = {{
      {fields_hex}
}};
inline const uint32_t CKEY_RAW_BITMAP = 0x{raw_bitmap:08x}u;
inline const std::vector<uint32_t> CKEY_RAW_INTS = {{{raw_ints_cpp}}};
inline const char* CKEY_TRAILING_HEX = "{h(trailing)}";
inline const char* CKEY_TEA_KEY_HEX  = "{h(tea_key)}";
inline const char* CKEY_XOR_KEY_HEX  = "{h(xor_key)}";
inline const char* CKEY_RAND2_HEX    = "{h(rand2)}";   // os.urandom(2)
inline const uint8_t CKEY_HEAD_RAND1 = 0x{head_rand1:02x}u;  // os.urandom(1)[0]
inline const char* CKEY_EXPECTED     = "{ckey}";

// ── DECODE vector — cKey CAPTURE thật ({REQUEST_PATH.name}) ──────────────────
// decode_ckey(CAP_CKEY) phải cho ra ĐÚNG các trường dưới đây (so với Python
// parse_full_captured). cKey capture có pad ngẫu nhiên nên KHÔNG roundtrip được
// — roundtrip được kiểm trên CKEY_EXPECTED (do encoder của ta sinh, pad=0).
inline const char* CAP_CKEY            = "{cap_ckey}";
inline const char* CAP_PREAMBLE_HEX    = "{h(d_pre)}";
inline const std::vector<std::string> CAP_FIELDS_HEX = {{
      {d_fields_hex}
}};
inline const uint32_t CAP_RAW_BITMAP   = 0x{d_bitmap:08x}u;
inline const std::vector<uint32_t> CAP_RAW_INTS = {{{d_ints_cpp}}};
inline const char* CAP_TRAILING_HEX    = "{h(d_trailing)}";
inline const int CAP_PAD_COUNT         = {d_meta['pad_count']};
inline const uint16_t CAP_TLV_LEN      = {d_meta['tlv_len']}u;
inline const uint32_t CAP_STORED_CS    = 0x{d_meta['stored_cs']:08x}u;
inline const uint32_t CAP_COMPUTED_CS  = 0x{d_meta['computed_cs']:08x}u;

// ── session_template loader vector — fixtures/session_template.sample.json ────
// SessionTemplate::parse(...) phải khôi phục ĐÚNG các giá trị dưới (so với Python
// SessionTemplate.load). query_params/headers phải GIỮ THỨ TỰ.
inline const int TPL_SCHEMA_VERSION = {st.schema_version};
inline const double TPL_CAPTURED_AT = {st.captured_at!r};
inline const char* TPL_APP_VER      = "{st.app_ver}";
inline const char* TPL_SOURCE_PATH  = "{st.source_request_path}";
inline const char* TPL_XOR_KEY_HEX  = "{st.xor_key_hex}";
inline const char* TPL_TEA_KEY_HEX  = "{st.tea_key_hex}";
inline const char* TPL_PREAMBLE_HEX = "{st.preamble_hex}";
inline const char* TPL_TRAILING_HEX = "{st.trailing_hex}";
inline const uint32_t TPL_RAW_BITMAP = 0x{st.raw_bitmap:08x}u;
inline const std::vector<uint32_t> TPL_RAW_INTS = {{{tpl_ints}}};
inline const char* TPL_GUID        = "{st.guid}";
inline const char* TPL_QIMEI36     = "{st.qimei36}";
inline const char* TPL_REQUEST_URL = "{st.request_url}";
inline const char* TPL_COOKIES     = "{st.cookies}";
inline const std::vector<std::string> TPL_QUERY_KEYS = {{
      {tpl_qkeys}
}};
inline const std::vector<std::string> TPL_QUERY_VALS = {{
      {tpl_qvals}
}};
inline const std::vector<std::string> TPL_HEADER_KEYS = {{
      {tpl_hkeys}
}};
inline const std::vector<std::string> TPL_HEADER_VALS = {{
      {tpl_hvals}
}};

// ── capability + constants vector (Pha 3) ────────────────────────────────────
// serialize_params(...) của C++ phải khớp các chuỗi key=value (mỗi dòng một cặp)
// dưới đây (sinh từ Python capability_flags / getvinfo_constants).
// <NULL> = key bị xóa khỏi body.
inline const char* CAPS_DEFAULT      = "{cpp_lit(caps_default)}";
inline const char* CAPS_FROM_TPL     = "{cpp_lit(caps_from_tpl)}";
inline const char* CAPS_VIDEO        = "{cpp_lit(caps_video)}";
inline const char* CAPS_BASIC        = "{cpp_lit(caps_basic)}";
inline const char* CAPS_UHD_NO_VVC   = "{cpp_lit(caps_uhd_no_vvc)}";
inline const char* CAPS_NO_DRM       = "{cpp_lit(caps_no_drm)}";
inline const char* CAPS_HDR_UHDOFF   = "{cpp_lit(caps_hdr_uhdoff)}";
inline const char* CONST_SAFE_STATIC = "{cpp_lit(const_safe)}";
inline const char* CONST_PATH_NORMAL = "{cpp_lit(const_path_normal)}";
inline const char* CONST_PATH_HISTORY= "{cpp_lit(const_path_history)}";

// ── Pha 4: getvinfo_params ────────────────────────────────────────────────────
// flowid
inline const char* FLOWID_UUID       = "{flow_uuid}";
inline const uint64_t FLOWID_NANO    = {flow_nano}ull;
inline const char* FLOWID_PLATFORM   = "{flow_platform}";
inline const char* FLOWID_EXPECTED   = "{flowid_expected}";
inline const uint64_t FLOWID_NANO2   = {flow_nano2}ull;
inline const char* FLOWID_EXPECTED2  = "{flowid_expected2}";
// encode_body (URL-encode kiểu Java)
inline const char* ENCBODY_EXPECTED  = "{cpp_lit(encbody_expected)}";
// generate_ckey tất định (fixture ckey, inject param2/rand21/tv_usec/pad)
inline const char* CKEY_GEN_PARAM2_HEX = "{h(inj_param2)}";
inline const char* CKEY_GEN_RAND21_HEX = "{h(inj_rand21)}";
inline const char* CKEY_GEN_DD_RANDOM8_HEX = "{h(ckey_dd_random8)}";
inline const uint32_t CKEY_GEN_TVUSEC  = {inj_tvusec}u;
inline const char* CKEY_GEN_RAND2_HEX  = "{h(ckey_rand2)}";
inline const uint8_t CKEY_GEN_HEAD1    = 0x{ckey_head1:02x}u;
inline const char* CKEY_GEN_VID        = "TESTVID123";
inline const char* CKEY_GEN_EXPECTED   = "{ckey_gen}";
// build_query_params (flowid/ckey cố định)
inline const char* BUILD_FIXED_FLOW    = "{FIXED_FLOW}";
inline const char* BUILD_FIXED_CKEY    = "{FIXED_CKEY}";
inline const char* BUILD_NORMAL        = "{cpp_lit(build_normal)}";
inline const char* BUILD_NORMAL_BODY   = "{cpp_lit(build_normal_body)}";
inline const char* BUILD_HISTORY       = "{cpp_lit(build_history)}";

}}  // namespace vec
"""
    out_path = HERE / "vectors" / "generated_vectors.h"
    out_path.write_text(out, encoding="utf-8")
    print(f"[OK] đã ghi {out_path}")
    print(f"     cKey expected len = {len(ckey)}")
    print(f"     rand2={rand2.hex()} head_rand1={head_rand1:02x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
