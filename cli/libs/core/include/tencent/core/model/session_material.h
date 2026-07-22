// session_material.h — Vật liệu phiên: túi khoá-giá trị mô tả THIẾT BỊ + TÀI
// KHOẢN mà mọi bộ dựng request đều đọc.
//
// Vì sao là map chuỗi chứ không phải struct chặt: các trường được lấy về từ
// nhiều nguồn khác nhau (MMKV, getprop, dumpsys, response server) và mỗi loại
// request lại cần một tập con khác nhau; native cũng tra cứu theo tên. Bộ dựng
// (protocol/trpc/head.h) quyết định THỨ TỰ field trên wire, không phải map này.
//
// Trước đây kiểu này tên là `trpc::Session` và nằm trong header của tầng
// protocol, khiến model/account.h phải phụ thuộc ngược lên protocol. Nay nó
// nằm đúng tầng model: dữ liệu thuần, không phụ thuộc gì.
//
// Nguồn tham chiếu: ../analytics/app/login_refresh_head.py (_pick).
#pragma once

#include <initializer_list>
#include <map>
#include <string>

namespace tencent::model {

struct SessionMaterial {
  std::map<std::string, std::string> kv;

  bool has(const std::string& key) const;

  // Giá trị của `key`, hoặc `fallback` nếu không có.
  std::string get(const std::string& key, const std::string& fallback = "") const;

  // Giá trị KHÁC RỖNG đầu tiên trong danh sách khoá — tương ứng `_pick` của
  // Python (login_refresh_head.py:43-57). Dùng khi cùng một thông tin có thể
  // nằm dưới nhiều tên khoá tuỳ nguồn dump.
  std::string pick(std::initializer_list<const char*> keys,
                   const std::string& fallback = "") const;

  long get_int(const std::string& key, long fallback) const;

  void set(const std::string& key, const std::string& value) { kv[key] = value; }
};

}  // namespace tencent::model
