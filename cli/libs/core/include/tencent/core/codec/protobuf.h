// pb_codec.h — Protobuf codec trung thực (parse -> list field -> re-serialize
// byte-exact). Port của ../analytics/app/pb_codec.py.
//
// Khác decoder gắn-schema: giữ NGUYÊN thứ tự field, field trùng, field lạ, nên
// decode->encode là identity. Dùng để vá từng field trong qqlive_head mà không
// phá phần còn lại (xem trpc_head.{h,cpp}).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tencent::pb {

// wire: 0=varint, 1=i64, 2=len-delimited, 5=i32 (../analytics/app/pb_codec.py:43-58).
struct Field {
  int num = 0;
  int wire = 0;
  uint64_t intval = 0;   // hợp lệ khi wire ∈ {0,1,5}
  std::string data;      // hợp lệ khi wire == 2 (bytes thô)

  static Field varint(int num, uint64_t v) { return Field{num, 0, v, {}}; }
  static Field i64(int num, uint64_t v) { return Field{num, 1, v, {}}; }
  static Field i32(int num, uint64_t v) { return Field{num, 5, v, {}}; }
  static Field bytes(int num, std::string d) { return Field{num, 2, 0, std::move(d)}; }

  std::string encode() const;  // ../analytics/app/pb_codec.py:48-58
};

// ../analytics/app/pb_codec.py:28-39 — varint LEB128 (mask 64-bit).
std::string write_varint(uint64_t n);
// ../analytics/app/pb_codec.py:15-25 — đọc varint tại offset `o`, cập nhật `o`.
uint64_t read_varint(const std::string& b, std::size_t& o);

// ../analytics/app/pb_codec.py:61-95
std::vector<Field> parse(const std::string& b);
std::string serialize(const std::vector<Field>& fields);
const Field* get_first(const std::vector<Field>& fields, int num);
std::vector<const Field*> get_all(const std::vector<Field>& fields, int num);

}  // namespace tencent::pb
