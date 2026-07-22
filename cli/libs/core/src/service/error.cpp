// error.cpp — Xem include/tencent/core/service/error.h.
#include "tencent/core/service/error.h"

namespace tencent::service {

int exit_code_for(ErrorCode code) {
  switch (code) {
    case ErrorCode::kOk:
      return 0;
    case ErrorCode::kInvalidArgument:
      return 2;   // quy ước quen thuộc: dùng sai cú pháp/tham số
    default:
      return 1;
  }
}

}  // namespace tencent::service
