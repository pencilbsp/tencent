// list_command.cpp — Lệnh `list`: liệt kê các account đã lưu.
#include "../commands.h"
#include "../console.h"

#include "tencent/core/service/account_service.h"

namespace tencent::cli {

int run_list() {
  const service::AccountService service;
  auto result = service.list();
  if (!result) return report(result.error());

  const auto& accounts = result.value();
  if (accounts.empty()) {
    print("(chưa có account nào trong " + service.accounts_directory() + ")");
    return 0;
  }
  for (const auto& account : accounts) {
    if (!account.readable) {
      print("  (không đọc được " + account.vuid + ")");
      continue;
    }
    // Dấu '*' đánh dấu account mặc định — lệnh khác dùng account này khi không
    // truyền --account.
    print(std::string(account.is_default ? "* " : "  ") + account.vuid +
          "  thiết bị=" + (account.model.empty() ? "?" : account.model) +
          " app=" + (account.app_ver.empty() ? "?" : account.app_ver) +
          " qimei36=" + account.qimei36.substr(0, 12) + "...");
  }
  return 0;
}

}  // namespace tencent::cli
