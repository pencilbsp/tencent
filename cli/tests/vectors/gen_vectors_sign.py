#!/usr/bin/env python3
"""
gen_vectors_sign.py — NGUỒN SỰ THẬT cho test byte-exact của login-refresh vusession
(login_sign energy MAC + login_refresh body/frame/response). Xuất
tests/generated_vectors_sign.h (namespace `vsign`, committed). Xem AGENTS.md §1.

TÁCH RIÊNG khỏi gen_vectors.py (drift capability) — như gen_vectors_acct.py. Chỉ
import module CHƯA drift: energy_port, login_sign, login_refresh_proto,
login_refresh, login_refresh_response.

Chạy:  python3 tests/gen_vectors_sign.py
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent   # tests/
APP_DIR = HERE.parent.parent / "analytics" / "app"
sys.path.insert(0, str(APP_DIR))

import energy_port as ep  # noqa: E402
from login_sign import build_original_data, sign_info  # noqa: E402
from login_refresh_proto import build_refresh_request  # noqa: E402
from login_refresh import build_refresh_frame  # noqa: E402
from login_refresh_response import parse_refresh_response  # noqa: E402
import pb_codec as pb  # noqa: E402


def h(b: bytes) -> str:
    return b.hex()


def cpp_lit(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


# ── Input tổng hợp (KHÔNG secret — toàn giá trị giả) ──────────────────────────
GUID = "2c3dc9b826a711f191697c8c09268fca"
VUREFRESH = "BM7wbOcTP0bmBrreiuCd8tlWeYKi_eAiLEOTI02z217_ARwKlXZd"
VUID = 3578420805
VUSESSION = "FAKEVUSESSION0001"
WX_APP_ID = "wxca942bbff22e0e51"
WX_OPEN_ID = "oQFqrjnUC6bBM6pP7YVBozFngFqs"
WX_ACCESS_TOKEN = "104_fakeaccesstoken_abc"
WX_REFRESH_TOKEN = "fakerefreshtoken_xyz"
QIMEI36 = "7011ecf5235e409f06ae191410001891a317"
USID = "1780051580769225"
NOW_MS = 1780099999000
RND = 1864727676
USEC = 123456


def main() -> int:
    # ── (1) energy / sign — usec INJECT để tất định (energy_port.py) ───────────
    energy_msgs = [
        b"abc",
        b"A" * 100,
        b"g=2c3dc9b826a711f191697c8c09268fca&v=AAAA&t=1780099999000&r=111",
        b"g=2c3dc9b826a711f191697c8c09268fca&v=ZZZZ&t=1780099999000&r=999",
        build_original_data(GUID, VUREFRESH, NOW_MS, RND).encode(),
    ]
    energy_usecs = [0, 1, 2, 1000, 123456]
    energy_cases = []  # (msg_hex, usec, energy_hex, sign)
    for msg in energy_msgs:
        for usec in energy_usecs:
            e = ep.energy(msg, usec=usec)
            energy_cases.append((h(msg), usec, h(e), ep.sign(msg, usec=usec)))

    # nonce derivation tất định (localize lỗi nếu energy sai).
    nonce_cases = [(u, h(ep.derive_nonce(u))) for u in (0, 1, 2, 1000, 123456)]

    # ── (2) build_refresh_request body (proto3 omit-identity) ─────────────────
    od = build_original_data(GUID, VUREFRESH, NOW_MS, RND)
    sig = ep.sign(od.encode(), usec=USEC)  # = sign_info nhưng tất định qua usec
    body = build_refresh_request(
        vuid=VUID, vusession=VUSESSION, vurefresh=VUREFRESH,
        wx_app_id=WX_APP_ID, wx_open_id=WX_OPEN_ID,
        wx_access_token=WX_ACCESS_TOKEN, wx_refresh_token=WX_REFRESH_TOKEN,
        qimei36=QIMEI36, original_data=od, sign=sig,
    )

    # ── (3) build_refresh_frame đầy đủ — Python truyền sign tường minh (tất định),
    #        C++ truyền usec + sign rỗng → tự tính = phải khớp. ────────────────
    frame = build_refresh_frame(
        vuid=VUID, vusession=VUSESSION, vurefresh=VUREFRESH,
        wx_app_id=WX_APP_ID, wx_open_id=WX_OPEN_ID,
        wx_access_token=WX_ACCESS_TOKEN, wx_refresh_token=WX_REFRESH_TOKEN,
        qimei36=QIMEI36, guid=GUID, usid=USID,
        now_ms=NOW_MS, rnd=RND, sign=sig,
    )

    # ── (4) parse RefreshResponse — dựng response tổng hợp rồi parse ──────────
    new_vusession = "NEWSESSION_abcdef0123456789"
    new_vurefresh = "NEWREFRESH_zyxwvu9876543210"
    new_access = "104_newaccesstoken_qwerty"
    new_refresh = "newrefreshtoken_uiop"
    third = pb.serialize([
        pb.Field(1, 0, 2),                       # third_type = WX
        pb.Field(2, 2, WX_APP_ID.encode()),
        pb.Field(3, 2, WX_OPEN_ID.encode()),
        pb.Field(4, 2, new_access.encode()),
        pb.Field(5, 2, new_refresh.encode()),
    ])
    resp_body = pb.serialize([
        pb.Field(1, 0, VUID),
        pb.Field(2, 2, new_vusession.encode()),
        pb.Field(3, 0, 1780200000),              # expire ts (giây)
        pb.Field(4, 2, new_vurefresh.encode()),
        pb.Field(5, 2, third),
    ])
    resp_head = pb.serialize([pb.Field(4, 0, 0)])  # trpc ret = 0 (OK)
    total = 16 + len(resp_head) + len(resp_body)
    resp_frame = (struct.pack(">H", 0x2352) + b"\x00\x00" + struct.pack(">I", total)
                  + struct.pack(">H", len(resp_head)) + b"\x00\x00" + b"\x00\x00\x00\x00"
                  + resp_head + resp_body)
    parsed = parse_refresh_response(resp_frame)  # sanity Python
    assert parsed.vusession == new_vusession and parsed.trpc_ret == 0

    # ── Sinh header C++ ───────────────────────────────────────────────────────
    energy_rows = ",\n".join(
        f'      {{"{m}", {u}u, "{e}", "{s}"}}' for (m, u, e, s) in energy_cases
    )
    nonce_rows = ",\n".join(f'      {{{u}u, "{n}"}}' for (u, n) in nonce_cases)

    out = f"""// AUTO-GENERATED bởi tests/gen_vectors_sign.py — KHÔNG SỬA TAY.
// Nguồn ground-truth: ../analytics/app/energy_port.py, login_sign.py, login_refresh_proto.py,
// login_refresh.py, login_refresh_response.py.
// Sinh lại: python3 tests/gen_vectors_sign.py   (AGENTS.md §1)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vsign {{

// ── (1) energy / sign (usec INJECT) ───────────────────────────────────────────
struct EnergyCase {{ const char* msg_hex; uint32_t usec; const char* energy_hex; const char* sign; }};
inline const std::vector<EnergyCase> ENERGY_CASES = {{
{energy_rows}
}};
struct NonceCase {{ uint32_t usec; const char* nonce_hex; }};
inline const std::vector<NonceCase> NONCE_CASES = {{
{nonce_rows}
}};

// ── (2) build_refresh_request body ────────────────────────────────────────────
inline const int64_t     VUID            = {VUID};
inline const char* VUSESSION         = "{VUSESSION}";
inline const char* VUREFRESH         = "{VUREFRESH}";
inline const char* WX_APP_ID         = "{WX_APP_ID}";
inline const char* WX_OPEN_ID        = "{WX_OPEN_ID}";
inline const char* WX_ACCESS_TOKEN   = "{WX_ACCESS_TOKEN}";
inline const char* WX_REFRESH_TOKEN  = "{WX_REFRESH_TOKEN}";
inline const char* QIMEI36           = "{QIMEI36}";
inline const char* GUID              = "{GUID}";
inline const char* USID              = "{USID}";
inline const uint64_t NOW_MS         = {NOW_MS}ull;
inline const uint32_t RND            = {RND}u;
inline const uint32_t USEC           = {USEC}u;
inline const char* ORIGINAL_DATA     = "{cpp_lit(od)}";
inline const char* SIGN              = "{sig}";
inline const char* REFRESH_BODY_HEX  = "{h(body)}";

// ── (3) build_refresh_frame đầy đủ ────────────────────────────────────────────
inline const char* REFRESH_FRAME_HEX = "{h(frame)}";

// ── (4) parse RefreshResponse ─────────────────────────────────────────────────
inline const char* RESP_FRAME_HEX        = "{h(resp_frame)}";
inline const int64_t RESP_VUID           = {VUID};
inline const char* RESP_VUSESSION         = "{new_vusession}";
inline const int64_t RESP_EXPIRE_TS      = 1780200000;
inline const char* RESP_VUREFRESH         = "{new_vurefresh}";
inline const char* RESP_ACCESS_TOKEN      = "{new_access}";
inline const char* RESP_REFRESH_TOKEN     = "{new_refresh}";
inline const int64_t RESP_TRPC_RET       = 0;

}}  // namespace vsign
"""
    out_path = HERE / "vectors" / "generated_vectors_sign.h"
    out_path.write_text(out, encoding="utf-8")
    print(f"[OK] đã ghi {out_path} ({len(energy_cases)} energy case)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
