// test_pb_codec.cpp — Test byte-exact cho pb_codec (Pha A).
//
// So write_varint + roundtrip parse/serialize với ground-truth Python
// (../analytics/app/pb_codec.py) trong generated_vectors.h. Xem AGENTS.md §0.
#include <cstdio>
#include <string>
#include <vector>

#include "vectors/generated_vectors_acct.h"
#include "tencent/core/codec/protobuf.h"

namespace pb = tencent::pb;

static int g_failures = 0;
static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}

// hex <-> std::string (bytes) cục bộ — pb_codec dùng std::string cho bytes.
static std::string from_hex(const std::string& hex) {
  auto nib = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
  };
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
    out.push_back(static_cast<char>((nib(hex[i]) << 4) | nib(hex[i + 1])));
  return out;
}
static std::string to_hex(const std::string& b) {
  static const char* d = "0123456789abcdef";
  std::string out;
  for (unsigned char c : b) { out.push_back(d[c >> 4]); out.push_back(d[c & 0xF]); }
  return out;
}

int main() {
  std::printf("== Pha A: pb_codec ==\n");

  // ── write_varint từng giá trị ──────────────────────────────────────────
  for (std::size_t i = 0; i < vacc::PB_VARINT_VALS.size(); ++i) {
    std::string got = to_hex(pb::write_varint(vacc::PB_VARINT_VALS[i]));
    bool ok = got == vacc::PB_VARINT_HEX[i];
    check(ok, "write_varint");
    if (!ok) std::printf("    val=%llu want=%s got=%s\n",
                         (unsigned long long)vacc::PB_VARINT_VALS[i],
                         vacc::PB_VARINT_HEX[i].c_str(), got.c_str());
  }

  // ── roundtrip parse->serialize byte-exact ──────────────────────────────
  std::string blob = from_hex(vacc::PB_BLOB_HEX);
  std::vector<pb::Field> fields = pb::parse(blob);
  std::string rt = pb::serialize(fields);
  bool rt_ok = rt == blob;
  check(rt_ok, "serialize(parse(blob)) == blob (identity)");
  if (!rt_ok) {
    std::printf("    want: %s\n", vacc::PB_BLOB_HEX);
    std::printf("    got : %s\n", to_hex(rt).c_str());
  }

  // ── cấu trúc: 5 field, giữ thứ tự + field trùng num ────────────────────
  check(fields.size() == 5, "parse -> 5 field");
  check(pb::get_all(fields, 1).size() == 2, "get_all(1) -> 2 (field trùng num)");

  const pb::Field* f1 = pb::get_first(fields, 1);
  check(f1 != nullptr && f1->wire == 0 && f1->intval == 150, "get_first(1) = varint 150");

  const pb::Field* f2 = pb::get_first(fields, 2);
  check(f2 != nullptr && f2->wire == 2 && f2->data == "hi", "get_first(2) = bytes 'hi'");

  const pb::Field* f3 = pb::get_first(fields, 3);
  check(f3 != nullptr && f3->wire == 5 && f3->intval == 0x01020304u, "get_first(3) = i32");

  // ── message lồng (f5) parse lại được ───────────────────────────────────
  const pb::Field* f5 = pb::get_first(fields, 5);
  bool f5_ok = f5 != nullptr && f5->wire == 2 && to_hex(f5->data) == vacc::PB_F5_INNER_HEX;
  check(f5_ok, "get_first(5) = message lồng (hex khớp)");
  if (f5) {
    std::vector<pb::Field> inner = pb::parse(f5->data);
    const pb::Field* in1 = pb::get_first(inner, 1);
    const pb::Field* in2 = pb::get_first(inner, 2);
    check(in1 && in1->intval == 7 && in2 && in2->data == "x", "f5 inner: {1:7, 2:'x'}");
  }

  std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
