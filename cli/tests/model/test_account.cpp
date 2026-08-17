// test_account.cpp — Test phần parse/IO thuần của model::Account + storage
// (không cần thiết bị). Dùng cookie + prefs XML TỔNG HỢP (không chứa bí mật).
// Phần adb là I/O thiết bị nên không test ở đây (AGENTS.md: dump ≠ byte-exact).
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "tencent/core/common/hex.h"
#include "tencent/core/model/account.h"
#include "tencent/core/storage/account_store.h"
#include "tencent/core/storage/paths.h"

namespace acc = tencent::account;

static int g_fail = 0;
static void check(bool ok, const std::string& name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) ++g_fail;
}
static void eq(const std::string& got, const std::string& want, const std::string& name) {
  bool ok = got == want;
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name.c_str());
  if (!ok) { ++g_fail; std::printf("    want=%s got=%s\n", want.c_str(), got.c_str()); }
}
static std::string read_text(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream out;
  out << in.rdbuf();
  return out.str();
}

int main() {
  std::printf("== account_file ==\n");

  // Cookie tổng hợp (giá trị giả) đủ marker + mọi field nhóm A/B/C.
  std::string cookie =
      "main_login=wx; app_ver=9.02.90.31109; video_platform=1000005; "
      "vuserid=3578420805; vusession=FAKE_VUSESSION_001; v_vurefresh=FAKE_VUREFRESH_002; "
      "appid=wxca942bbff22e0e51; openid=oFAKEopenid; access_token=FAKE_ACCESS; "
      "refresh_token=FAKE_REFRESH; vdevice_qimei36=7011ecf5235e409f06ae191410001891a317; "
      "guid=2c3dc9b826a711f191697c8c09268fca; usid=1780051580769225";

  auto c = acc::parse_cookie(cookie);
  eq(c["vuserid"], "3578420805", "parse_cookie vuserid");
  eq(c["v_vurefresh"], "FAKE_VUREFRESH_002", "parse_cookie v_vurefresh");
  eq(c["vdevice_qimei36"], "7011ecf5235e409f06ae191410001891a317", "parse_cookie qimei36");

  acc::Account a;
  acc::fill_from_cookie(a, c);
  eq(a.vuid, "3578420805", "fill vuid");
  eq(a.vusession, "FAKE_VUSESSION_001", "fill vusession");
  eq(a.vurefresh, "FAKE_VUREFRESH_002", "fill vurefresh (v_vurefresh)");
  eq(a.wx_app_id, "wxca942bbff22e0e51", "fill wx_app_id");
  eq(a.wx_open_id, "oFAKEopenid", "fill wx_open_id");
  eq(a.wx_access_token, "FAKE_ACCESS", "fill wx_access_token");
  eq(a.wx_refresh_token, "FAKE_REFRESH", "fill wx_refresh_token");
  eq(a.qimei36, "7011ecf5235e409f06ae191410001891a317", "fill qimei36");
  eq(a.guid, "2c3dc9b826a711f191697c8c09268fca", "fill guid");
  eq(a.usid, "1780051580769225", "fill usid");
  check(acc::missing_required(a).find("vuid") == std::string::npos, "vuid không thiếu");
  check(acc::missing_required(a).find("device.model") != std::string::npos,
        "account thiếu fingerprint model bị báo lỗi");

  std::string built_cookie = acc::build_login_cookie(a, 1781000000123LL);
  check(built_cookie.find("ctime=1781000000123;zdtime=1781000000123;") !=
            std::string::npos,
        "build_login_cookie refresh ctime/zdtime");
  check(built_cookie.find("vuserid=3578420805;vusession=FAKE_VUSESSION_001;") !=
            std::string::npos,
        "build_login_cookie current video auth");
  check(built_cookie.find("v_t_access_token=FAKE_ACCESS;") != std::string::npos,
        "build_login_cookie current WX token");
  check(built_cookie.find("us_stamp=1780051580769;") != std::string::npos,
        "build_login_cookie derives install-session stamp");

  // find_login_cookie_in_prefs: bọc cookie trong prefs XML (có entity &amp;).
  std::string xml =
      "<?xml version='1.0'?><map>"
      "<string name=\"other\">khong phai cookie</string>"
      "<string name=\"login\">main_login=wx; vusession=AA&amp;bb; "
      "vdevice_qimei36=QMEI; guid=G; usid=U; vuserid=42; v_vurefresh=R; "
      "appid=AP; openid=OP; access_token=AC; refresh_token=RF</string>"
      "</map>";
  std::string found = acc::find_login_cookie_in_prefs(xml);
  check(found.find("vusession=AA&bb") != std::string::npos, "find cookie + unescape &amp;");

  // find_login_cookie_in_binary: MMKV blob — cookie kẹp giữa byte non-printable,
  // có decoy ngắn + run rác. Phải lấy run dài nhất đủ 2 marker.
  std::string mmkv;
  mmkv += std::string("\x00\x01\x02", 3);
  mmkv += "rac_ngan_khong_marker";
  mmkv += std::string("\x00\xff", 2);
  mmkv += "vusession=SHORT; vdevice_qimei36=Q";           // decoy ngắn
  mmkv += std::string("\x00", 1);
  std::string real = "main_login=wx; vusession=REALSESSIONVALUE; v_vurefresh=RR; "
                     "vdevice_qimei36=QMEI36VALUE; guid=GG; usid=UU; vuserid=99";
  mmkv += std::string("\x07", 1) + real + std::string("\x00\x00", 2);
  std::string bcookie = acc::find_login_cookie_in_binary(mmkv);
  eq(bcookie, real, "find_login_cookie_in_binary lấy run dài nhất");
  acc::Account ba;
  acc::fill_from_cookie(ba, acc::parse_cookie(bcookie));
  eq(ba.vusession, "REALSESSIONVALUE", "binary→fill vusession");
  eq(ba.vuid, "99", "binary→fill vuid");

  // ── parse_account_json (NGUỒN CHUẨN) ──────────────────────────────────────
  // Blob nhị phân: noise + account JSON cũ (ctime nhỏ) + account JSON mới + key_guid.
  std::string aj_old =
      "{\"accountType\":1,\"openId\":\"OLDOPEN\",\"accessToken\":\"OLDACCESS\","
      "\"refreshToken\":\"MBS\",\"videoUserId\":3578420805,\"videoSessionKey\":\"OLDVS\","
      "\"videoRefreshKey\":\"OLDVR\",\"appid\":\"APPID\",\"nickName\":\"腾讯网友\","
      "\"headImgUrl\":\"http://x/y.png\",\"videoCreateTime\":1000,\"videoExpireTime\":7199748}";
  std::string aj_new =
      "{\"accountType\":1,\"openId\":\"oFAKEopenid\",\"accessToken\":\"NEWACCESSTOKEN_111\","
      "\"refreshToken\":\"MBS\",\"videoUserId\":3578420805,\"videoSessionKey\":\"NEWSESSION165\","
      "\"videoRefreshKey\":\"NEWREFRESH118\",\"appid\":\"wxca942bbff22e0e51\","
      "\"nickName\":\"腾讯网友\",\"headImgUrl\":\"http://h/u.png\","
      "\"videoCreateTime\":1780710604252,\"videoExpireTime\":7199748}";
  std::string ablob;
  ablob += std::string("\x00\x01rac", 5);
  ablob += aj_old;
  ablob += std::string("\x00key_guid\x21 ", 11);
  ablob += "bfaf6ff75e6e11f191697c8c09268fca";   // UUID v1
  ablob += std::string("\x00", 1);
  ablob += aj_new;                                 // mới hơn → phải thắng
  ablob += std::string("\x00", 1);

  acc::Account ja;
  check(acc::parse_account_json(ablob, ja), "parse_account_json tìm thấy");
  check(ja.login_type == 1, "account JSON lưu login_type WX");
  eq(ja.vuid, "3578420805", "account JSON vuid");
  eq(ja.vusession, "NEWSESSION165", "account JSON chọn videoCreateTime mới nhất");
  eq(ja.vurefresh, "NEWREFRESH118", "account JSON vurefresh");
  eq(ja.wx_access_token, "NEWACCESSTOKEN_111", "account JSON accessToken");
  eq(ja.wx_refresh_token, "MBS", "account JSON refreshToken=MBS (đúng, không cụt)");
  eq(ja.wx_app_id, "wxca942bbff22e0e51", "account JSON appid");
  eq(ja.wx_open_id, "oFAKEopenid", "account JSON openId");
  eq(ja.nickname, "腾讯网友", "account JSON nickName");
  check(ja.vusession_expire_ts == (1780710604252LL + 7199748LL) / 1000, "expire_ts tính từ ctime+exp");

  // extract_guid: lấy 32-hex v1 sau key_guid.
  eq(acc::extract_guid(ablob), "bfaf6ff75e6e11f191697c8c09268fca", "extract_guid (key_guid→v1)");
  check(acc::extract_guid("no marker here").empty(), "extract_guid rỗng nếu không có key_guid");

  // find_login_cookie_for_vuid: 2 run khác vuserid → chọn đúng account.
  std::string mblob;
  mblob += std::string("\x00", 1);
  mblob += "vusession=AAA; vdevice_qimei36=Q1; vuserid=111; guid=G1";
  mblob += std::string("\x00", 1);
  mblob += "vusession=BBB; vdevice_qimei36=Q2; vuserid=222; guid=G2";
  mblob += std::string("\x00", 1);
  std::string pick222 = acc::find_login_cookie_for_vuid(mblob, "222");
  check(pick222.find("vuserid=222") != std::string::npos, "find_login_cookie_for_vuid chọn đúng vuid");

  // device de-hardcode: to_session phải mang model/app_ver/platform.
  a.model = "SM-X910"; a.brand = "samsung"; a.app_ver = "9.99.0.12345";
  a.manufacturer = "samsung"; a.android_api_level = "35";
  a.app_build = "12345"; a.app_sub_build = "12345";
  a.platform = "1000005"; a.sdtfrom = "v5000"; a.os_version = "15";
  a.screen_width = 1752; a.screen_height = 2800; a.screen_dpi = 360;
  a.turing.package_uid = 10215;
  a.turing.android_user_id = 0;
  a.turing.k26_uuid_dashed = "849db351-83e7-4ff4-918c-94459ffbfc79";
  a.turing.k26_wire = "849db35183e74ff4918c94459ffbfc79";
  a.turing.k26_source = "root-cache";
  a.turing.k34_native_tail = "EDB:1_2_18,NRF:-1_-1_-1_0_-1,NSF:0_-1_-1_0_-1,UD:0";
  a.turing.k34_cl = "57_29";
  a.turing.k34_r = "0_1";
  a.turing.k34_us_503 = 1;
  a.turing.k34_us_502 = 10215;
  a.turing.k34_stif = "";
  a.turing.k34_nrf = {-1, 0, -1, -1, -1};
  a.turing.k34_nsf = {-1, 0, -1, -1, 0};
  a.k26_save_status_raw = {0, 0, 0, 0, 0};
  std::string k26_blob = tencent::turing::encode_k26_cache(a.turing.k26_uuid_dashed);
  a.turing.k26_nodes.push_back(
      {1, "/data/data/com.tencent.qqlive/app_turingdfp/log/.t.log",
       static_cast<int64_t>(k26_blob.size()), 0, 0,
       a.turing.k26_uuid_dashed, tencent::util::hex_encode(k26_blob)});
  a.turing.k34_cl_cache_hex = "00391c201d";
  a.turing_prefs_xml =
      "<?xml version=\"1.0\"?><map>"
      "<string name=\"204\">0_1</string>"
      "<int name=\"503\" value=\"1\"/>"
      "<int name=\"502\" value=\"10215\"/>"
      "<string name=\"904\"></string>"
      "</map>";
  a.raw_fingerprint_json =
      "{\"schema\":\"turing-raw/1\","
      "\"props\":{\"ro.product.model\":\"SM-X910\","
      "\"ro.product.brand\":\"samsung\","
      "\"ro.product.manufacturer\":\"samsung\","
      "\"ro.build.version.release\":\"15\","
      "\"ro.build.version.sdk\":\"35\"},"
      "\"device\":{\"screen_width\":1752,\"screen_height\":2800,\"screen_dpi\":360},"
      "\"app\":{\"qqlive_uid\":10215,\"android_user_id\":0,"
      "\"qqlive_version_name\":\"9.99.0.12345\",\"qqlive_version_code\":12345}}";
  auto sess = a.to_session();
  check(acc::missing_required(a).find("device.model") == std::string::npos,
        "account dùng model từ fingerprint");
  eq(sess.get("model"), "SM-X910", "to_session model (de-hardcode)");
  eq(sess.get("app_ver"), "9.99.0.12345", "to_session app_ver");
  eq(sess.get("android_api_level"), "35", "to_session android_api_level");
  eq(sess.get("screen_width"), "1752", "to_session screen_width");
  eq(sess.get("vusession"), "FAKE_VUSESSION_001", "to_session vusession");

  acc::Account non_wx = a;
  non_wx.login_type = -1;
  non_wx.wx_app_id.clear();
  non_wx.wx_open_id.clear();
  non_wx.wx_access_token.clear();
  non_wx.wx_refresh_token.clear();
  check(acc::missing_required(non_wx).find("wx_") == std::string::npos,
        "account không WeChat không bắt buộc wx_*");
  eq(non_wx.to_session().get("login_type"), "2", "legacy non-WX infers phone login_type");

  std::string phone_json =
      "{\"accountType\":2,\"videoUserId\":2468,\"videoSessionKey\":\"PHONEVS\","
      "\"videoRefreshKey\":\"PHONEVR\",\"nickName\":\"phone user\","
      "\"videoCreateTime\":2000,\"videoExpireTime\":6000}";
  acc::Account phone;
  check(acc::parse_account_json(phone_json, phone), "parse_account_json phone tìm thấy");
  check(phone.login_type == 2, "account JSON lưu login_type PHONE");
  eq(phone.to_session().get("login_type"), "2", "to_session login_type phone");

  acc::Account partial_wx = non_wx;
  partial_wx.wx_open_id = "oFAKEopenid";
  std::string partial_missing = acc::missing_required(partial_wx);
  check(partial_missing.find("wx_app_id") != std::string::npos &&
            partial_missing.find("wx_access_token") != std::string::npos &&
            partial_missing.find("wx_refresh_token") != std::string::npos,
        "account WeChat thiếu token vẫn bị báo lỗi");

  // Roundtrip bundle save→load (ghi thư mục tạm).
  namespace fs = std::filesystem;
  fs::path tmp = fs::temp_directory_path() / "tencent_acct_test_bundle";
  fs::remove_all(tmp);
  a.source_method = "from-prefs"; a.source_device_id = "test"; a.created_at = 111;
  tencent::storage::save_account(tmp.string(), a);
  check(fs::is_regular_file(tmp / "auth.json"), "bundle có auth.json");
  check(fs::is_regular_file(tmp / "fingerprint.json"), "bundle có fingerprint.json");
  std::string auth_text = read_text(tmp / "auth.json");
  std::string fingerprint_text = read_text(tmp / "fingerprint.json");
  check(auth_text.find("qimei36") == std::string::npos,
        "auth.json không chứa fingerprint identity");
  check(auth_text.find("\"device\"") == std::string::npos,
        "auth.json không chứa device derived");
  check(fingerprint_text.find("\"schema\": \"turing-raw/1\"") != std::string::npos,
        "fingerprint.json giữ raw helper snapshot");
  acc::Account b = tencent::storage::load_account(tmp.string());
  eq(b.vuid, a.vuid, "roundtrip vuid");
  check(b.login_type == a.login_type, "roundtrip login_type");
  eq(b.vurefresh, a.vurefresh, "roundtrip vurefresh");
  eq(b.model, a.model, "roundtrip device.model");
  eq(b.app_ver, a.app_ver, "roundtrip device.app_ver");
  eq(b.android_api_level, a.android_api_level, "roundtrip device.android_api_level");
  eq(b.turing.k26_wire, a.turing.k26_wire, "roundtrip turing.k26_wire");
  eq(b.turing.k26_source, "root-cache", "roundtrip turing.k26_source");
  eq(b.turing.k34_native_tail, a.turing.k34_native_tail, "roundtrip turing.k34_native_tail");
  eq(b.turing.k34_r, "0_1", "roundtrip turing.k34_r");
  check(b.turing.k34_us_503 == 1 && b.turing.k34_us_502 == 10215,
        "roundtrip turing K34 US prefs");
  check(b.k26_save_status_raw == std::vector<int>({0, 0, 0, 0, 0}),
        "roundtrip raw K26 save status");
  check(b.turing.k26_nodes.size() == 5 &&
            b.turing.k26_nodes[1].blob_hex == tencent::util::hex_encode(k26_blob),
        "roundtrip raw turing.k26_nodes");
  check(b.created_at == 111, "roundtrip created_at");
  check(b.screen_width == 1752, "roundtrip screen_width");
  check(tencent::storage::account_exists(tmp.string()), "exists nhận account bundle");
  fs::perms auth_perms = fs::status(tmp / "auth.json").permissions();
  check((auth_perms & (fs::perms::group_all | fs::perms::others_all)) == fs::perms::none,
        "auth.json không cấp quyền group/other");
  fs::remove_all(tmp);

  // Loader accepts the legacy `<path>.json` when the bundle directory is absent.
  fs::path legacy_base = fs::temp_directory_path() / "tencent_acct_legacy";
  fs::path legacy_file = legacy_base;
  legacy_file += ".json";
  {
    std::ofstream out(legacy_file, std::ios::binary | std::ios::trunc);
    out << "{\"schema_version\":1,\"vuid\":\"42\","
           "\"identity\":{\"qimei36\":\"LEGACY_Q\"},"
           "\"credential\":{\"vusession\":\"LEGACY_S\"},"
           "\"device\":{\"model\":\"LEGACY_MODEL\"}}";
  }
  acc::Account legacy = tencent::storage::load_account(legacy_base.string());
  eq(legacy.vuid, "42", "legacy fallback vuid");
  eq(legacy.qimei36, "LEGACY_Q", "legacy fallback qimei36");
  eq(legacy.model, "LEGACY_MODEL", "legacy fallback device");
  check(tencent::storage::account_exists(legacy_base.string()), "exists nhận legacy <path>.json");
  fs::remove(legacy_file);

  std::printf("\n%s (%d failure(s))\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
  return g_fail == 0 ? 0 : 1;
}
