// main.cpp — Điểm vào của `tencent-cli`.
//
// Nhiệm vụ DUY NHẤT của file này: khai báo cú pháp dòng lệnh (CLI11), đổ tham
// số vào struct của từng lệnh, rồi gọi hàm tương ứng. Không có nghiệp vụ, không
// gọi mạng, không đụng đĩa — mọi thứ đó nằm trong tencent::core.
//
// Nhờ ranh giới này, một bản GUI chỉ cần gọi cùng những service mà các file
// commands/*.cpp đang gọi.
#include "CLI/CLI11.hpp"

#include "commands.h"
#include "console.h"

using namespace tencent;   // NOLINT: chỉ trong file điểm vào cho gọn

int main(int argc, char** argv) {
  CLI::App app{"tencent-cli — công cụ dòng lệnh cho Tencent Video"};
  app.require_subcommand(1);

  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "In thêm thông tin gỡ lỗi");

  // ── auth ───────────────────────────────────────────────────────────────────
  // Ba chế độ loại trừ nhau, phân biệt bằng cờ nào XUẤT HIỆN trên dòng lệnh.
  // Cả ba đều nhận giá trị TÙY CHỌN: `--adb` không kèm gì thì tự chọn thiết bị,
  // `--refresh`/`--info` không kèm gì thì dùng account mặc định.
  cli::AuthOptions auth_options;
  auto* auth = app.add_subcommand(
      "auth", "Lấy credential từ thiết bị (--adb), làm mới phiên (--refresh) "
              "hoặc xem thông tin (--info)");
  auto* adb_flag =
      auth->add_option("--adb", auth_options.device_id,
                       "Lấy dữ liệu qua adb; có thể kèm <device_id>")
          ->expected(0, 1);
  std::string refresh_account, info_account;
  auto* refresh_flag =
      auth->add_option("--refresh", refresh_account,
                       "Làm mới vusession cho <account_id> (mặc định: account mặc định)")
          ->expected(0, 1);
  auto* info_flag =
      auth->add_option("--info", info_account,
                       "Xem thông tin + VIP của <account_id> (mặc định: account mặc định)")
          ->expected(0, 1);
  bool auth_no_refresh = false;
  auth->add_flag("--dry-run", auth_options.dry_run,
                 "refresh: chỉ dựng khung request, không gửi");
  auth->add_flag("--local", auth_options.local_only,
                 "info: chỉ hiển thị dữ liệu cục bộ, không gọi mạng");
  auth->add_flag("--no-refresh", auth_no_refresh,
                 "info: không tự làm mới vusession khi hết hạn");
  auth->add_option("--timeout", auth_options.timeout_sec, "Thời gian chờ HTTP (giây)")
      ->capture_default_str();
  auth->add_option("--pkg", auth_options.package, "Tên gói ứng dụng")
      ->capture_default_str();
  auth->add_option("--from-prefs", auth_options.from_prefs_file,
                   "Đọc dữ liệu app đã lấy sẵn (ngoại tuyến, không cần adb)");
  auth->add_flag("--no-device", auth_options.skip_device_info,
                 "Bỏ qua phần đọc thuộc tính thiết bị");
  auth->add_flag("--set-default", auth_options.set_default,
                 "Đặt account này làm mặc định");
  auth->add_option("--out", auth_options.output_path,
                   "Ghi vào thư mục khác (mặc định: accounts/<vuid>/)");
  auth_options.package = account::kDefaultPkg;

  // ── list ───────────────────────────────────────────────────────────────────
  auto* list = app.add_subcommand("list", "Liệt kê các account đã lưu");

  // ── getvinfo ───────────────────────────────────────────────────────────────
  cli::GetvinfoOptions gv;
  auto* getvinfo =
      app.add_subcommand("getvinfo", "Lấy URL phát video theo --vid/--url");
  getvinfo->add_option("--url", gv.url,
                       "URL trang xem; tự tách --cid/--vid từ /x/cover/<cid>/<vid>.html");
  getvinfo->add_option("--vid", gv.vid, "Mã video Tencent");
  getvinfo->add_option("--account", gv.account_id,
                       "Account dùng để gọi; mặc định là account mặc định");
  getvinfo->add_option("--defn", gv.defn, "Chất lượng (uhd/hd/sd/shd/fhd...)")
      ->capture_default_str();
  getvinfo->add_option("--mode", gv.mode, "normal|history")->capture_default_str();
  getvinfo->add_option("--cid", gv.cid, "Mã bộ/series");
  getvinfo->add_flag("--enable-vvc", gv.capabilities.enable_vvc, "Bật khả năng VVC/H.266");
  getvinfo->add_flag("--enable-hdr,--enable-hdr10", gv.capabilities.enable_hdr,
                     "Bật khả năng HDR10");
  getvinfo->add_flag("--enable-hdr-enhance", gv.capabilities.enable_hdr_enhance,
                     "Bật khả năng HDR Enhance");
  getvinfo->add_flag("--enable-hdr-vivid", gv.capabilities.enable_hdr_vivid,
                     "Bật khả năng HDR Vivid");
  getvinfo->add_flag("--enable-drm", gv.capabilities.enable_drm,
                     "Bật DRM theo đúng khả năng thiết bị");
  getvinfo->add_option("--enable-dolby", gv.capabilities.dolby,
                       "Bật Dolby: profile_a|profile_b|both");
  getvinfo->add_option("--enable-hdr-fps", gv.capabilities.hdr_fps,
                       "Bật HDR10 theo bậc fps: 60fps|50fps|30fps|24fps");
  getvinfo->add_flag("--dry-run", gv.dry_run, "Chỉ dựng request, không gửi");
  getvinfo->add_flag("--raw", gv.print_raw_json, "In nguyên JSON phản hồi");
  getvinfo->add_option("--request-out", gv.request_out,
                       "Ghi request đầy đủ để dùng lại với lệnh download");
  getvinfo->add_option("--response-out", gv.response_out, "Ghi phản hồi thô");
  getvinfo->add_option("--decoded-out", gv.decoded_out, "Ghi JSON sau khi giải ANC");
  getvinfo->add_option("-o,--output", gv.output_path,
                       "Tải và giải mã thẳng ra .mp4/.ts sau khi lấy thông tin");
  getvinfo->add_option("--work-dir", gv.download.work_dir,
                       "Thư mục cache; mặc định ~/.tencent");
  getvinfo->add_option("--video-index", gv.download.video_index, "Vị trí trong vl.vi[]")
      ->capture_default_str();
  auto* gv_workers =
      getvinfo->add_option("--workers", gv.download.workers,
                           "Số luồng tải segment; bỏ qua để tải tuần tự")
          ->capture_default_str();
  getvinfo->add_option("--retries", gv.download.retries, "Số lần thử lại mỗi segment")
      ->capture_default_str();
  getvinfo->add_option("--getplay-sleep", gv.download.getplay_sleep_sec,
                       "Số giây nghỉ giữa hai segment getPlayTs")
      ->capture_default_str();
  getvinfo->add_option("--remain-seconds", gv.download.remain_seconds,
                       "Scheduler remainSeconds_0x8A4 để tính emergency_level")
      ->check(CLI::Range(-1, 2147483647))
      ->capture_default_str();
  getvinfo->add_flag("--simulate-player-buffer", gv.download.simulate_player_buffer,
                     "Mô phỏng bộ đệm trình phát để tính emergency_level");
  getvinfo->add_option("--max-buffer-length", gv.download.max_buffer_length,
                       "Bộ đệm mô phỏng tối đa (giây)")
      ->check(CLI::PositiveNumber)
      ->capture_default_str();
  getvinfo->add_option("--download-speed", gv.download.download_speed,
                       "Tốc độ phát trong mô phỏng (1.0 = 1x)")
      ->check(CLI::PositiveNumber)
      ->capture_default_str();
  getvinfo->add_flag("--segment-query", gv.download.segment_query,
                     "Thêm cost/cdn_strategy/dlipscene vào URL segment như native");
  getvinfo->add_flag("--download-subtitle", gv.download.download_subtitle,
                     "Tải phụ đề từ sfl.fi[] nếu phản hồi có");
  auto* gv_subtitle_format =
      getvinfo->add_option("--subtitle-format", gv.download.subtitle_format,
                           "Định dạng phụ đề: vtt|srt|ass")
          ->check(CLI::IsMember({"vtt", "srt", "ass"}))
          ->capture_default_str();
  getvinfo->add_flag("--trace", gv.download.trace, "In chi tiết các bước giải mã");
  getvinfo->add_flag("--insecure", gv.insecure,
                     "Bỏ kiểm tra TLS (cần cho vv6.video.qq.com)");
  getvinfo->add_option("--proxy", gv.proxy_url,
                       "Proxy: ss:// (SIP002) hoặc http(s)/socks4/socks5");
  getvinfo->add_option("--timeout", gv.timeout_sec, "Thời gian chờ HTTP (giây)")
      ->capture_default_str();

  // ── download ───────────────────────────────────────────────────────────────
  cli::DownloadOptions dl;
  dl.download.timeout = 30;
  auto* download_cmd =
      app.add_subcommand("download", "Tải/giải mã video từ --request + --response đã lưu");
  download_cmd->add_option("--request", dl.download.request_path, "File request đã lưu")
      ->required();
  download_cmd->add_option("--response", dl.download.response_path,
                           "File JSON phản hồi getvinfo đã lưu")
      ->required();
  download_cmd->add_option("-o,--output", dl.download.output_path,
                           "Đường dẫn .mp4/.ts; không bắt buộc khi dùng --no-download");
  download_cmd->add_option("--work-dir", dl.download.work_dir,
                           "Thư mục làm việc/cache; mặc định ~/.tencent");
  download_cmd->add_option("--video-index", dl.download.video_index, "Vị trí trong vl.vi[]")
      ->capture_default_str();
  auto* dl_workers =
      download_cmd->add_option("--workers", dl.download.workers,
                               "Số luồng tải/biến đổi; bỏ qua để chạy tuần tự")
          ->capture_default_str();
  download_cmd->add_option("--timeout", dl.download.timeout, "Thời gian chờ HTTP (giây)")
      ->capture_default_str();
  download_cmd->add_option("--retries", dl.download.retries, "Số lần thử lại mỗi segment")
      ->capture_default_str();
  download_cmd->add_option("--getplay-sleep", dl.download.getplay_sleep_sec,
                           "Số giây nghỉ giữa hai segment getPlayTs")
      ->capture_default_str();
  download_cmd->add_option("--remain-seconds", dl.download.remain_seconds,
                           "Scheduler remainSeconds_0x8A4 để tính emergency_level")
      ->check(CLI::Range(-1, 2147483647))
      ->capture_default_str();
  download_cmd->add_flag("--simulate-player-buffer", dl.download.simulate_player_buffer,
                         "Mô phỏng bộ đệm trình phát để tính emergency_level");
  download_cmd->add_option("--max-buffer-length", dl.download.max_buffer_length,
                           "Bộ đệm mô phỏng tối đa (giây)")
      ->check(CLI::PositiveNumber)
      ->capture_default_str();
  download_cmd->add_option("--download-speed", dl.download.download_speed,
                           "Tốc độ phát trong mô phỏng (1.0 = 1x)")
      ->check(CLI::PositiveNumber)
      ->capture_default_str();
  download_cmd->add_flag("--segment-query", dl.download.segment_query,
                         "Thêm cost/cdn_strategy/dlipscene vào URL segment như native");
  download_cmd->add_flag("--download-subtitle", dl.download.download_subtitle,
                         "Tải phụ đề từ sfl.fi[] nếu có");
  auto* dl_subtitle_format =
      download_cmd->add_option("--subtitle-format", dl.download.subtitle_format,
                               "Định dạng phụ đề: vtt|srt|ass")
          ->check(CLI::IsMember({"vtt", "srt", "ass"}))
          ->capture_default_str();
  download_cmd->add_option("--user-agent", dl.download.download_user_agent,
                           "User-Agent cho segment; mặc định lấy theo account");
  download_cmd->add_option("--account", dl.account_id,
                           "Account để đọc phiên bản DownloadProxy");
  download_cmd->add_option("--proxy", dl.download.proxy_url,
                           "Proxy cho appcfg: ss:// hoặc http(s)/socks4/socks5");
  download_cmd->add_flag("--insecure", dl.download.insecure, "Bỏ kiểm tra TLS cho appcfg");
  download_cmd->add_option("--anc-rsa-key", dl.download.anc.rsa_private_key_path,
                           "Thay khoá RSA PEM dùng giải ANC rc");
  download_cmd->add_option("--ckey-tea-key", dl.download.anc.ckey_tea_key_hex,
                           "Thay khoá QQ-TEA của cKey (hex)");
  download_cmd->add_option("--ckey-xor-key", dl.download.anc.ckey_xor_key_hex,
                           "Thay khoá XOR của cKey (hex)");
  download_cmd->add_flag("--no-download", dl.download.no_download,
                         "Chỉ giải mã và in thông tin, không tải segment");
  download_cmd->add_flag("--trace", dl.download.trace, "In chi tiết các bước giải mã");

  // ── probe-url ──────────────────────────────────────────────────────────────
  cli::ProbeOptions probe_options;
  auto* probe = app.add_subcommand("probe-url", "Thử một request GET; hỗ trợ --proxy ss://");
  probe->add_option("--url", probe_options.url, "URL cần GET")->required();
  probe->add_option("--proxy", probe_options.proxy_url,
                    "Proxy: ss:// hoặc http(s)/socks4/socks5");
  probe->add_option("--timeout", probe_options.timeout_sec, "Thời gian chờ HTTP (giây)")
      ->capture_default_str();
  probe->add_option("-H,--header", probe_options.headers,
                    "Header dạng 'Name: value'; lặp lại được nhiều lần");
  probe->add_flag("--insecure", probe_options.insecure, "Bỏ kiểm tra TLS");

  // ── flush ──────────────────────────────────────────────────────────────────
  cli::FlushOptions flush_options;
  auto* flush = app.add_subcommand("flush", "Xoá cache video trong thư mục làm việc");
  flush->add_option("--work-dir", flush_options.work_dir, "Gốc cache; mặc định ~/.tencent");
  flush->add_option("--vid", flush_options.vid, "Chỉ xoá cache của một mã video");
  flush->add_option("--defn", flush_options.defn, "Chỉ xoá một chất lượng; cần kèm --vid");
  flush->add_flag("--dry-run", flush_options.dry_run, "Chỉ in đường dẫn sẽ bị xoá");

  CLI11_PARSE(app, argc, argv);
  cli::install_console_log_sink(verbose);

  if (list->parsed()) return cli::run_list();

  if (auth->parsed()) {
    if (refresh_flag->count() > 0) {
      auth_options.mode = cli::AuthOptions::Mode::kRefresh;
      auth_options.account_id = refresh_account;
    } else if (info_flag->count() > 0) {
      auth_options.mode = cli::AuthOptions::Mode::kInfo;
      auth_options.account_id = info_account;
      auth_options.allow_refresh = !auth_no_refresh;
    } else {
      auth_options.mode = cli::AuthOptions::Mode::kDump;
      auth_options.use_device = adb_flag->count() > 0;
    }
    return cli::run_auth(auth_options);
  }

  if (getvinfo->parsed()) {
    if (!cli::resolve_ids_from_url(gv.vid, gv.cid, gv.url)) {
      if (gv.url.empty() && gv.vid.empty()) {
        cli::print_error("getvinfo cần --vid hoặc --url");
      }
      return 2;
    }
    // CLI11 không phân biệt "người dùng gõ giá trị mặc định" với "không gõ gì",
    // nên hai cờ dưới đây phải hỏi count() — pipeline có hành vi khác nhau.
    gv.download.workers_explicit = gv_workers->count() > 0;
    gv.download.download_subtitle =
        gv.download.download_subtitle || gv_subtitle_format->count() > 0;
    return cli::run_getvinfo(gv);
  }

  if (download_cmd->parsed()) {
    dl.download.workers_explicit = dl_workers->count() > 0;
    dl.download.download_subtitle =
        dl.download.download_subtitle || dl_subtitle_format->count() > 0;
    return cli::run_download(dl);
  }

  if (probe->parsed()) return cli::run_probe(probe_options);
  if (flush->parsed()) return cli::run_flush(flush_options);
  return 0;
}
