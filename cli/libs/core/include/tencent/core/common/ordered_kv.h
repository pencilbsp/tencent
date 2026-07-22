// ordered_kv.h — Cặp key-value GIỮ NGUYÊN THỨ TỰ CHÈN.
//
// Vì sao không dùng std::map: body getvinfo và header TRPC có thứ tự key CỐ
// ĐỊNH (../analytics/app/plan_getvinfo_params.md §4.1). std::map sắp xếp theo
// khoá ⇒ sai thứ tự ⇒ server từ chối. Mọi chỗ dựng params/headers PHẢI dùng
// hai kiểu dưới đây.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tencent {

using OrderedKV = std::vector<std::pair<std::string, std::string>>;

// Như OrderedKV nhưng giá trị có thể VẮNG. std::nullopt mang đúng ngữ nghĩa
// `None` của Python: "xoá hẳn key này khỏi body" (dùng cho capability/extra).
using OptionalKV = std::vector<std::pair<std::string, std::optional<std::string>>>;

// ── Truy vấn/ghi giữ thứ tự (gom về đây để không lặp lại ở 5 module) ────────

// Trả con trỏ tới value của `key`, hoặc nullptr nếu không có.
const std::string* find_value(const OrderedKV& items, const std::string& key);

// Trả value hoặc `fallback` nếu thiếu/rỗng.
std::string value_or(const OrderedKV& items, const std::string& key,
                     const std::string& fallback = "");

// Ghi đè value nếu key đã tồn tại, ngược lại chèn vào CUỐI (giữ thứ tự cũ).
void set_value(OrderedKV& items, const std::string& key, const std::string& value);

// Bản OptionalKV: nullopt = đánh dấu xoá key khi hoà vào body.
void set_value(OptionalKV& items, const std::string& key,
               const std::optional<std::string>& value);

// Xoá key (nếu có). Trả true nếu đã xoá.
bool erase_key(OrderedKV& items, const std::string& key);

}  // namespace tencent
