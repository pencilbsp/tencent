// result.h — Kiểu trả về của TẦNG DỊCH VỤ: giá trị HOẶC lỗi, không ném.
//
// Vì sao cần: lõi sẽ được dùng bởi cả CLI lẫn GUI. Ném ngoại lệ qua biên
// (nhất là qua binding C/Qt/luồng nền) rất khó chịu, và CLI thì cần map lỗi
// sang mã thoát. Quy ước của dự án:
//
//   • Bên TRONG một module, ném std::runtime_error thoải mái (giữ nguyên cách
//     viết cũ của các bộ port thuật toán).
//   • Ở biên `tencent::service::*`, mọi ngoại lệ PHẢI được bắt và chuyển thành
//     Result<T> có Error. Xem service/error.h (hàm capture()).
//
// Result<void> (bí danh Status) dùng cho thao tác chỉ quan tâm thành/bại.
#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace tencent {

// Phân loại lỗi ở mức đủ thô để giao diện quyết định cách hiển thị/thoát.
enum class ErrorCode {
  kOk = 0,
  kInvalidArgument,   // tham số người dùng sai (mã thoát 2)
  kNotFound,          // không có account/file/vid
  kAuthExpired,       // vusession/vurefresh hết hạn — cần đăng nhập lại
  kPermissionDenied,  // thiếu root/quyền đọc dữ liệu riêng của app
  kDeviceUnavailable, // không có thiết bị adb / helper chưa cài
  kNetwork,           // lỗi transport (DNS, TLS, timeout)
  kProtocol,          // server trả mã lỗi hoặc dữ liệu không parse được
  kIo,                // đọc/ghi đĩa
  kUnsupported,       // chức năng chưa hỗ trợ trên nền tảng này
  kInternal,          // bug của ta
};

struct Error {
  ErrorCode code = ErrorCode::kInternal;
  std::string message;  // câu ngắn hiển thị cho người dùng
  std::string detail;   // ngữ cảnh kỹ thuật (raw response, exception what())

  Error() = default;
  Error(ErrorCode error_code, std::string error_message, std::string error_detail = "")
      : code(error_code),
        message(std::move(error_message)),
        detail(std::move(error_detail)) {}

  // Chuỗi một dòng để log/CLI in ra.
  std::string to_string() const {
    return detail.empty() ? message : message + " (" + detail + ")";
  }
};

// ── Result<T> ────────────────────────────────────────────────────────────────
template <typename T>
class Result {
 public:
  Result(T value) : storage_(std::move(value)) {}          // NOLINT: cho phép ngầm
  Result(Error error) : storage_(std::move(error)) {}      // NOLINT: cho phép ngầm

  bool ok() const { return std::holds_alternative<T>(storage_); }
  explicit operator bool() const { return ok(); }

  // Chỉ gọi khi ok(). Gọi sai = lỗi lập trình (std::get sẽ ném bad_variant_access).
  T& value() { return std::get<T>(storage_); }
  const T& value() const { return std::get<T>(storage_); }
  T* operator->() { return &std::get<T>(storage_); }
  const T* operator->() const { return &std::get<T>(storage_); }

  T value_or(T fallback) const { return ok() ? std::get<T>(storage_) : std::move(fallback); }

  // Chỉ gọi khi !ok().
  const Error& error() const { return std::get<Error>(storage_); }
  ErrorCode code() const { return ok() ? ErrorCode::kOk : error().code; }

 private:
  std::variant<T, Error> storage_;
};

// ── Result<void> ─────────────────────────────────────────────────────────────
template <>
class Result<void> {
 public:
  Result() = default;                                       // thành công
  Result(Error error) : error_(std::move(error)) {}         // NOLINT: cho phép ngầm

  bool ok() const { return !error_.has_value(); }
  explicit operator bool() const { return ok(); }
  const Error& error() const { return *error_; }
  ErrorCode code() const { return ok() ? ErrorCode::kOk : error_->code; }

 private:
  std::optional<Error> error_;
};

// Thao tác không trả dữ liệu.
using Status = Result<void>;

// Tiện dụng: tạo Result lỗi mà không phải gõ kiểu.
inline Error make_error(ErrorCode code, std::string message, std::string detail = "") {
  return Error(code, std::move(message), std::move(detail));
}

}  // namespace tencent
