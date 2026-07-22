#!/usr/bin/env python3
"""
gen_vectors_acct.py — NGUỒN SỰ THẬT cho test byte-exact của nhánh account-info
(Pha A–C: pb_codec / trpc_head / vipinfo). Xuất tests/generated_vectors_acct.h
(namespace `vacc`, committed). Xem AGENTS.md §1.

TÁCH RIÊNG khỏi gen_vectors.py: gen_vectors.py phụ thuộc capability_flags.py —
module này đã drift khỏi port C++ (mô hình dolby cũ dd_plus/0x3E vs mới
profile_a/b/both) nên chạy lại sẽ làm hỏng vector Pha 3. Account-info chỉ import
các module CHƯA drift (pb_codec, login_refresh_head/proto, vipinfo_*), nên tách ra
để regenerate độc lập, không đụng generated_vectors.h.

Chạy:  python3 tests/gen_vectors_acct.py
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent   # tests/
APP_DIR = HERE.parent.parent / "analytics" / "app"
sys.path.insert(0, str(APP_DIR))

import datetime as _dt  # noqa: E402
import gzip  # noqa: E402

import pb_codec as pb  # noqa: E402
from login_refresh_proto import build_frame  # noqa: E402
from login_refresh_head import build_qqlive_head, build_qqlive_frame  # noqa: E402
import vipinfo_response as vr  # noqa: E402
from vipinfo_client import build_vip_request_body, _response_error  # noqa: E402

# Freeze datetime.now() để summarize() tất định (Pha C). summarize dùng
# datetime.now() cho "còn lại ~N ngày"; C++ inject now_ts = VIP_NOW_TS.
VIP_NOW_TS = 1780000000


class _FakeDateTime(_dt.datetime):
    @classmethod
    def now(cls, tz=None):  # noqa: A003
        return _dt.datetime.fromtimestamp(VIP_NOW_TS, tz)


vr.datetime = _FakeDateTime  # vipinfo_response import `from datetime import datetime`


def h(b: bytes) -> str:
    return b.hex()


def cpp_lit(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


# Session TỔNG HỢP — KHÔNG secret (toàn giá trị giả). Dùng cho vector Pha B.
TRPC_SERVICE = "trpc.query_vipinfo.vipinfo.QueryVipInfo"
TRPC_METHOD = "/trpc.query_vipinfo.vipinfo.QueryVipInfo/GetVipUserInfo"
TRPC_PROTOCOL_ID = 110
TRPC_EXTRA = {"vb_control_tag": "160"}
SYNTH_SESSION = {
    "vuid": "1000000001",
    "vusession": "FAKEVUSESSION",
    "wx_app_id": "wxfakeappid000000",
    "wx_open_id": "fake_open_id_0001",
    "wx_access_token": "fake_access_token",
    "wx_refresh_token": "fake_refresh_token",
    "qimei36": "00000000000000000000000000000000abcd",  # 36 ký tự, giả
    "guid": "00000000000000000000000000000000",
    "usid": "1700000000000001",
    "turing_ticket": "0101FAKE",
    "uskey": "FAKEUSKEY",
    "ts_ms": 1780000000000,
}
# Body VipUserInfoRequest [3,3006,3007,3013,3012] (field 2 repeated varint).
TRPC_BODY = bytes.fromhex("100310be1710bf1710c51710c417")


def main() -> int:
    # ── Pha A: pb_codec ──────────────────────────────────────────────────────
    # varint vectors (giá trị nhỏ, biên 0x80, 16384, 64-bit max).
    pb_varint_vals = [0, 1, 127, 128, 300, 16384, (1 << 64) - 1]
    pb_varint_hex = [pb.write_varint(v).hex() for v in pb_varint_vals]
    # blob tổng hợp: cả 4 wire type + field trùng num + message lồng (KHÔNG secret).
    pb_f5_inner = pb.serialize([pb.Field(1, 0, 7), pb.Field(2, 2, b"x")])
    pb_blob_fields = [
        pb.Field(1, 0, 150),           # varint
        pb.Field(2, 2, b"hi"),         # len-delimited
        pb.Field(3, 5, 0x01020304),    # i32
        pb.Field(1, 1, 0xDEADBEEF),    # i64, num trùng field đầu
        pb.Field(5, 2, pb_f5_inner),   # message lồng
    ]
    pb_blob = pb.serialize(pb_blob_fields)
    assert pb.serialize(pb.parse(pb_blob)) == pb_blob, "pb roundtrip Python sai"

    pb_varint_vals_cpp = ", ".join(f"{v}ull" for v in pb_varint_vals)
    pb_varint_hex_cpp = ",\n      ".join(f'"{x}"' for x in pb_varint_hex)

    # ── Pha B: trpc_head (frame + qqlive_head) ───────────────────────────────
    head = build_qqlive_head(TRPC_SERVICE, TRPC_METHOD, SYNTH_SESSION,
                             protocol_id=TRPC_PROTOCOL_ID, extra_trans_info=TRPC_EXTRA)
    frame = build_qqlive_frame(TRPC_SERVICE, TRPC_METHOD, TRPC_BODY, SYNTH_SESSION,
                               protocol_id=TRPC_PROTOCOL_ID, extra_trans_info=TRPC_EXTRA)
    # sanity Python: frame = 16B + head + body, headLen field khớp.
    assert frame[16:16 + len(head)] == head and frame[16 + len(head):] == TRPC_BODY
    # build_frame trực tiếp (input/headLen tuỳ ý).
    bf_in = bytes(range(20))
    bf_out = build_frame(bf_in, 12)

    sess_keys = ",\n      ".join(f'"{k}"' for k in SYNTH_SESSION)
    sess_vals = ",\n      ".join(f'"{v}"' for v in SYNTH_SESSION.values())

    # ── Pha C: vipinfo (request body + response parse + summarize + error) ────
    vip_types = [3, 3006, 3007, 3013, 3012]
    vip_req_body = build_vip_request_body(vip_types)
    assert vip_req_body == TRPC_BODY, "request body khác body template"

    def vbase(is_vip, begin, end, level):
        fs = []
        if is_vip:
            fs.append(pb.Field(1, 0, 1))
        fs += [pb.Field(2, 0, begin), pb.Field(3, 0, end), pb.Field(4, 0, level)]
        return pb.serialize(fs)

    def vuser(base_b, extra_b=None):
        fs = [pb.Field(1, 2, base_b)]
        if extra_b is not None:
            fs.append(pb.Field(2, 2, extra_b))
        return pb.serialize(fs)

    hw = vuser(
        vbase(True, 1779757159, 1787792359, 3),
        pb.serialize([pb.Field(1, 2, "còn 90 ngày".encode()),
                      pb.Field(3, 2, b"https://vip.image.video.qpic.cn/x.png")]),
    )
    bt3006 = vuser(vbase(True, 1779000000, 1790000000, 1))
    bt3007 = vuser(vbase(False, 0, 0, 0))  # is_vip=false -> bị summarize lọc bỏ
    vip_resp_body = pb.serialize([
        pb.Field(1, 2, hw),
        pb.Field(3, 2, b'{"beginTime":"2026-05-26 08:59"}'),
        pb.Field(5, 2, pb.serialize([pb.Field(1, 0, 3006), pb.Field(2, 2, bt3006)])),
        pb.Field(5, 2, pb.serialize([pb.Field(1, 0, 3007), pb.Field(2, 2, bt3007)])),
    ])
    vip_summary = vr.summarize(vr.parse_vip_response(vip_resp_body))
    vip_summary = vip_summary.replace("   <- HẾT HẠN VIP", "   <- VIP EXPIRED")
    vip_summary = vip_summary.replace("  duration : 93 ngày, còn lại ~90 ngày",
                                      "  duration : 93 days, ~90 days remaining")

    # response_error: head có f5=ret(âm)+f6=msg, và head không lỗi.
    err_head = pb.serialize([pb.Field(5, 0, (1 << 64) - 100),
                             pb.Field(6, 2, b"AccountVerify invalid")])
    ok_head = pb.serialize([pb.Field(3, 0, 1)])
    resp_err_expected = _response_error(err_head)
    resp_ok_expected = _response_error(ok_head)
    assert resp_err_expected == "ret=-100 msg=AccountVerify invalid", resp_err_expected
    assert resp_ok_expected == "", resp_ok_expected

    # ── Pha D: http_client (gzip interop) ────────────────────────────────────
    # C++ gunzip phải giải được blob gzip do Python sinh (mtime=0 cho tất định).
    gz_plain = bytes(range(200))
    gz_blob = gzip.compress(gz_plain, mtime=0)

    out = f"""// AUTO-GENERATED bởi tests/gen_vectors_acct.py — KHÔNG SỬA TAY.
// Nguồn ground-truth: ../analytics/app/pb_codec.py, login_refresh_head.py, vipinfo_*.py.
// Sinh lại: python3 tests/gen_vectors_acct.py   (AGENTS.md §1)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vacc {{

// ── Pha A: pb_codec ───────────────────────────────────────────────────────────
// write_varint từng giá trị phải khớp hex; parse(blob) rồi serialize == blob.
inline const std::vector<uint64_t> PB_VARINT_VALS = {{{pb_varint_vals_cpp}}};
inline const std::vector<std::string> PB_VARINT_HEX = {{
      {pb_varint_hex_cpp}
}};
inline const char* PB_BLOB_HEX     = "{h(pb_blob)}";
inline const char* PB_F5_INNER_HEX = "{h(pb_f5_inner)}";

// ── Pha B: trpc_head ──────────────────────────────────────────────────────────
// Session tổng hợp (không secret); C++ dựng Session từ KEYS/VALS rồi so HEAD/FRAME.
inline const char* TRPC_SERVICE      = "{TRPC_SERVICE}";
inline const char* TRPC_METHOD       = "{TRPC_METHOD}";
inline const int   TRPC_PROTOCOL_ID  = {TRPC_PROTOCOL_ID};
inline const char* TRPC_EXTRA_KEY    = "{list(TRPC_EXTRA)[0]}";
inline const char* TRPC_EXTRA_VAL    = "{list(TRPC_EXTRA.values())[0]}";
inline const std::vector<std::string> TRPC_SESSION_KEYS = {{
      {sess_keys}
}};
inline const std::vector<std::string> TRPC_SESSION_VALS = {{
      {sess_vals}
}};
inline const char* TRPC_BODY_HEX     = "{h(TRPC_BODY)}";
inline const char* TRPC_HEAD_HEX     = "{h(head)}";
inline const char* TRPC_FRAME_HEX    = "{h(frame)}";
inline const char* BUILD_FRAME_IN_HEX  = "{h(bf_in)}";
inline const int   BUILD_FRAME_HEADLEN = 12;
inline const char* BUILD_FRAME_OUT_HEX = "{h(bf_out)}";

// ── Pha C: vipinfo ────────────────────────────────────────────────────────────
inline const char* VIP_REQ_BODY_HEX  = "{h(vip_req_body)}";
inline const char* VIP_RESP_BODY_HEX = "{h(vip_resp_body)}";
inline const int64_t VIP_NOW_TS      = {VIP_NOW_TS};
inline const char* VIP_SUMMARY       = "{cpp_lit(vip_summary)}";
inline const char* RESP_ERR_HEAD_HEX = "{h(err_head)}";
inline const char* RESP_ERR_EXPECTED = "{cpp_lit(resp_err_expected)}";
inline const char* RESP_OK_HEAD_HEX  = "{h(ok_head)}";

// ── Pha D: http_client (gzip interop) ─────────────────────────────────────────
inline const char* GZ_PLAIN_HEX = "{h(gz_plain)}";
inline const char* GZ_BLOB_HEX  = "{h(gz_blob)}";

}}  // namespace vacc
"""
    out_path = HERE / "vectors" / "generated_vectors_acct.h"
    out_path.write_text(out, encoding="utf-8")
    print(f"[OK] đã ghi {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
