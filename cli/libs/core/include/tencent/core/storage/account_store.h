// account_store.h — Đọc/ghi bundle account trên đĩa.
//
// Ranh giới với model/account.h: ở đó là DỮ LIỆU (struct Account + cách phân
// tích cookie/MMKV); ở đây là ĐỊNH DẠNG LƯU TRỮ. Nhờ tách đôi, việc đổi schema
// file không đụng tới thuật toán, và test có thể dựng Account trong bộ nhớ mà
// không cần chạm đĩa.
//
// Bố cục (schema v2, xem storage/paths.h):
//   <account_dir>/auth.json         — identity + credential + session
//   <account_dir>/fingerprint.json  — qimei/guid, turing-raw/1, K26/K34 thô
// File monolith cũ `<vuid>.json` (schema v1) vẫn ĐỌC được và sẽ tự chuyển sang
// bundle ở lần ghi kế tiếp.
//
// Mọi file được ghi với quyền 0600 và thư mục 0700 vì chứa credential.
#pragma once

#include <string>

#include "tencent/core/model/account.h"

namespace tencent::storage {

// `path` là THƯ MỤC account (cũng chấp nhận file `<vuid>.json` schema v1).
// Ném std::runtime_error nếu không đọc/parse được.
account::Account load_account(const std::string& path);

// Ghi bundle (tạo thư mục nếu cần). Ghi kiểu thay-thế-nguyên-tử để tiến trình
// bị ngắt giữa chừng không làm mất bản cũ.
void save_account(const std::string& path, const account::Account& account);

// Có bundle (hoặc file legacy) tại đường dẫn này không.
bool account_exists(const std::string& path);

}  // namespace tencent::storage
