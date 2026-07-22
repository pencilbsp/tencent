// helper_probe.cpp — Xem include/tencent/core/device/helper_probe.h.
#include "tencent/core/device/helper_probe.h"

#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

#include "tencent/core/common/hex.h"
#include "tencent/core/common/json.h"
#include "tencent/core/common/strings.h"
#include "tencent/core/device/adb_bridge.h"
#include "tencent/core/protocol/identity/turing_build.h"

namespace tencent::turing {
namespace {

std::string request_id() {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  std::mt19937_64 rng{std::random_device{}()};
  std::ostringstream out;
  out << std::hex << now << rng();
  return out.str();
}

std::string run_as_cat(const std::string& device_id, const std::string& package,
                       const std::string& path) {
  return adb::exec_out(device_id, "run-as " + adb::device_quote(package) +
                                " cat " + adb::device_quote(path));
}

std::vector<int> parse_csv_ints(const std::string& text) {
  std::vector<int> out;
  std::istringstream in(text);
  std::string item;
  while (std::getline(in, item, ',')) {
    if (item.empty()) continue;
    try {
      out.push_back(std::stoi(item));
    } catch (...) {
      out.push_back(-40002);
    }
  }
  return out;
}

}  // namespace

bool helper_installed(const std::string& device_id, const ProbeOptions& opt) {
  std::string out = adb::shell(
      device_id, "cmd package path " + adb::device_quote(opt.package));
  if (out.find("package:") == std::string::npos) return false;
  std::string dump =
      adb::shell(device_id, "dumpsys package " + adb::device_quote(opt.package));
  return dump.find("ProbeReceiver") != std::string::npos &&
         dump.find(opt.action) != std::string::npos;
}

HelperSnapshot collect_helper_snapshot(const std::string& device_id,
                                       const ProbeOptions& opt) {
  if (!helper_installed(device_id, opt)) {
    throw std::runtime_error(
        "Tencent Helper is not installed or does not support helper IPC (" +
        opt.package + ")");
  }

  HelperSnapshot snapshot;
  snapshot.request_id = request_id();
  std::string component = opt.package + "/" + opt.receiver;
  std::string response_dir = opt.response_root + "/" + snapshot.request_id;
  std::string broadcast =
      "am broadcast -a " + adb::device_quote(opt.action) +
      " -n " + adb::device_quote(component) +
      " --es request_id " + adb::device_quote(snapshot.request_id) +
      " --es target_pkg " + adb::device_quote(opt.target_package);
  adb::shell(device_id, broadcast);

  std::string status_text;
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(opt.timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    status_text = run_as_cat(device_id, opt.package, response_dir + "/status.json");
    if (status_text.find("\"state\":\"done\"") != std::string::npos ||
        status_text.find("\"state\":\"error\"") != std::string::npos) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(opt.poll_ms));
  }
  if (status_text.empty()) {
    throw std::runtime_error(
        "Tencent Helper timed out; open the helper and answer its root prompt");
  }

  nlohmann::json status;
  try {
    status = nlohmann::json::parse(status_text);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("invalid helper status: ") + e.what());
  }
  if (status.value("state", std::string()) != "done") {
    throw std::runtime_error("helper collection failed: " +
                             status.value("error", std::string("unknown error")));
  }

  snapshot.root_granted = status.value("root", std::string()) == "1";
  snapshot.k26_save_status =
      parse_csv_ints(status.value("save_status", std::string()));
  snapshot.raw_json =
      run_as_cat(device_id, opt.package, response_dir + "/device_raw.json");
  snapshot.account_blob =
      run_as_cat(device_id, opt.package, response_dir + "/account_mmkv.bin");
  snapshot.prefs_blob =
      run_as_cat(device_id, opt.package, response_dir + "/account_prefs.bin");
  snapshot.turing_prefs_blob =
      run_as_cat(device_id, opt.package, response_dir + "/turing_prefs.xml");
  snapshot.k34_cl_blob =
      run_as_cat(device_id, opt.package, response_dir + "/k34_cl.bin");
  snapshot.k26_blobs.resize(5);
  for (int i = 0; i < 5; ++i) {
    snapshot.k26_blobs[i] =
        run_as_cat(device_id, opt.package,
                   response_dir + "/k26_" + std::to_string(i) + ".bin");
  }

  adb::shell(device_id, "run-as " + adb::device_quote(opt.package) +
                            " rm -rf " + adb::device_quote(response_dir));
  if (snapshot.raw_json.find("\"schema\":\"turing-raw/1\"") ==
      std::string::npos) {
    throw std::runtime_error("helper returned no valid raw device snapshot");
  }
  return snapshot;
}

// ── Ảnh chụp → vật liệu Turing ──────────────────────────────────────────────
// Chuyển nguyên logic từ tencent_cli_main.cpp cũ (collect_turing_material /
// fill_turing_fallback): nó thuộc về tầng device chứ không phải tầng CLI.

Material material_from_snapshot(const HelperSnapshot& snapshot, const std::string& package,
                                const util::Json& raw) {
  Material material;
  const util::Json* app = raw.contains("app") ? &raw.at("app") : nullptr;
  material.package_uid = app ? util::json_int(*app, "qqlive_uid") : 0;
  material.android_user_id =
      material.package_uid >= 100000 ? static_cast<int>(material.package_uid / 100000) : 0;
  // Không có root ⇒ không đọc được cache riêng của app: chỉ giữ phần định danh.
  if (!snapshot.root_granted) return material;

  const auto prefs = parse_turing_prefs_xml(snapshot.turing_prefs_blob);
  const std::vector<std::string> paths = k26_cache_paths(package);
  // Cờ prefs chặn ĐỌC / GHI từng vị trí cache (chuỗi rỗng = không bị chặn).
  const char* load_gate[5] = {"2011", "", "2009", "2009", ""};
  const char* save_gate[5] = {"2010", "", "2008", "2008", ""};
  bool loaded = false;

  for (std::size_t i = 0; i < paths.size(); ++i) {
    const bool gated = load_gate[i][0] != '\0' && !pref_enabled(prefs, load_gate[i]);
    K26Node node;
    node.index = static_cast<int>(i);
    node.path = paths[i];
    if (gated) {
      node.read_status = -1;
      node.decode_status = -1;
    } else {
      const std::string& blob =
          i < snapshot.k26_blobs.size() ? snapshot.k26_blobs[i] : std::string();
      node.size = static_cast<int64_t>(blob.size());
      node.read_status = blob.empty() ? -11002 : 0;
      if (blob.empty()) {
        node.decode_status = -11002;
      } else {
        node.blob_hex = util::hex_encode(blob);
        const K26Decoded decoded = decode_k26_cache(blob);
        node.decode_status = decoded.status;
        node.uuid_dashed = decoded.uuid_dashed;
      }
      if (!loaded) {
        material.k34_nrf[i] = node.decode_status;
        if (node.decode_status == 0 && !node.uuid_dashed.empty()) {
          loaded = true;
          material.k26_uuid_dashed = node.uuid_dashed;
          material.k26_wire = k26_wire_from_uuid(node.uuid_dashed);
          material.k26_source = "root-cache";
          material.k26_cache_blob_hex = node.blob_hex;
          material.k34_load_bitmap |= (1 << static_cast<int>(i));
        }
      }
    }
    material.k26_nodes.push_back(node);
  }

  material.k34_edb_state = loaded ? 1 : 2;
  for (std::size_t i = 0; i < paths.size(); ++i) {
    const bool gated = save_gate[i][0] != '\0' && !pref_enabled(prefs, save_gate[i]);
    if (gated) {
      material.k34_nsf[i] = -1;
      continue;
    }
    const int status =
        i < snapshot.k26_save_status.size() ? snapshot.k26_save_status[i] : -40002;
    material.k34_nsf[i] = status;
    if (status == 0) material.k34_save_bitmap |= (1 << static_cast<int>(i));
  }
  material.k34_ud = material.android_user_id;
  material.k34_native_tail =
      build_k34_native_tail(material.k34_edb_state, material.k34_load_bitmap,
                            material.k34_save_bitmap, material.k34_nrf,
                            material.k34_nsf, material.k34_ud);

  if (!snapshot.k34_cl_blob.empty()) {
    material.k34_cl_cache_hex = util::hex_encode(snapshot.k34_cl_blob);
    if (const auto cl = decode_k34_cl_cache(snapshot.k34_cl_blob)) {
      material.k34_cl_a = cl->first;
      material.k34_cl_c = cl->second;
    }
  }
  if (material.k34_cl_a != 0 || material.k34_cl_c != 0) {
    material.k34_cl =
        std::to_string(material.k34_cl_a) + "_" + std::to_string(material.k34_cl_c);
  }

  const auto pref = [&prefs](const char* key) {
    const auto it = prefs.find(key);
    return it == prefs.end() ? std::string() : it->second;
  };
  material.k34_r = pref("204");
  material.k34_us_503 = static_cast<int>(util::to_int64(pref("503"), 0));
  material.k34_us_502 = static_cast<int>(util::to_int64(pref("502"), 0));
  material.k34_stif = pref("904");
  return material;
}

void apply_material_fallback(Material& material, const Material* previous) {
  if (material.k26_wire.empty()) {
    if (previous && !previous->k26_wire.empty()) {
      // Tái dùng UUID đã lưu: đổi K26 là đổi danh tính thiết bị trước server.
      material.k26_uuid_dashed = previous->k26_uuid_dashed;
      material.k26_wire = previous->k26_wire;
      material.k26_source = "persisted";
    } else {
      material.k26_wire = synth_k26();
      material.k26_uuid_dashed = normalize_k26_uuid(material.k26_wire);
      material.k26_source = "synthesized";
    }
  }
  if (!material.k34_native_tail.empty()) return;
  // Không đọc được cache nào: trạng thái "chưa nạp" (edb_state = 2).
  material.k34_edb_state = 2;
  material.k34_load_bitmap = 0;
  material.k34_save_bitmap = 0;
  material.k34_nrf.assign(5, -1);
  material.k34_nsf.assign(5, -1);
  material.k34_native_tail =
      build_k34_native_tail(2, 0, 0, material.k34_nrf, material.k34_nsf,
                            material.android_user_id);
}

}  // namespace tencent::turing
