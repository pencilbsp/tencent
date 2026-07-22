// error.h — Chuyển ngoại lệ thành Error ở BIÊN của tầng service.
//
// Quy ước (xem common/result.h): bên trong lõi các module vẫn ném ngoại lệ như
// cũ; nhưng không ngoại lệ nào được phép thoát ra khỏi `tencent::service::*`.
// Mọi phương thức public của service bọc thân hàm bằng `guard(...)`.
#pragma once

#include <exception>
#include <string>
#include <utility>

#include "tencent/core/common/result.h"

namespace tencent::service {

// Bọc một lời gọi có thể ném. `context` là câu mô tả việc đang làm, sẽ thành
// phần message của Error (ví dụ "không nạp được account").
//
//   return guard<Account>(ErrorCode::kIo, "không nạp được account", [&] {
//     return storage::load_account(path);
//   });
template <typename T, typename Callable>
Result<T> guard(ErrorCode code, const std::string& context, Callable&& body) {
  try {
    return Result<T>(std::forward<Callable>(body)());
  } catch (const std::invalid_argument& e) {
    // Sai tham số luôn là lỗi phía người dùng, bất kể `code` được truyền vào.
    return Error(ErrorCode::kInvalidArgument, context, e.what());
  } catch (const std::exception& e) {
    return Error(code, context, e.what());
  }
}

// Bản cho thao tác không trả dữ liệu.
template <typename Callable>
Status guard_status(ErrorCode code, const std::string& context, Callable&& body) {
  try {
    std::forward<Callable>(body)();
    return Status();
  } catch (const std::invalid_argument& e) {
    return Error(ErrorCode::kInvalidArgument, context, e.what());
  } catch (const std::exception& e) {
    return Error(code, context, e.what());
  }
}

// Mã thoát quy ước cho CLI: 0 thành công, 2 lỗi tham số, 1 còn lại.
int exit_code_for(ErrorCode code);

}  // namespace tencent::service
