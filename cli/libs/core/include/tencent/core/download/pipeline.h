#pragma once

#include <string>
#include <vector>

#include "tencent/core/download/proxy_config.h"
#include "tencent/core/common/json.h"

namespace tencent::download {

struct AncOptions {
  std::string profile = "auto";
  std::string rsa_private_key_path;
  std::string ckey_tea_key_hex;
  std::string ckey_xor_key_hex;
};

struct Options {
  std::string request_path;
  std::string response_path;
  std::string output_path;
  std::string work_dir;
  std::string vid;
  std::string defn;
  int video_index = 0;
  int workers = 8;
  bool workers_explicit = false;
  bool no_download = false;
  // Do not keep downloaded media or subtitle segments in the video cache after
  // producing the requested output. They are still staged temporarily so the
  // remux/decrypt pipeline can consume them.
  bool no_cache = false;
  long timeout = 30;
  int retries = 3;
  int getplay_sleep_sec = 1;
  int remain_seconds = -1;
  bool simulate_player_buffer = false;
  double max_buffer_length = 30.0;
  double download_speed = 1.0;
  bool segment_query = false;
  bool download_subtitle = false;
  std::string subtitle_format = "vtt";
  bool trace = false;
  AncOptions anc;
  std::string download_user_agent;
  download_proxy::Context proxy_config_context;
  std::string proxy_url;
  bool insecure = false;
};

// Default shared cache root for getvinfo -o and the standalone download command.
std::string default_work_dir();

struct CacheIdentity {
  std::string vid;
  std::string defn;
};

// Deterministic cache layout for one requested video quality:
// <root>/videos/<vid>/<defn>. Components are normalized before joining.
std::string cache_work_dir(const std::string& root, const std::string& vid,
                           const std::string& defn);

// Reads the form body from a serialized/curl getvinfo request. This supports
// standalone `download`, where VID/quality are not passed as CLI arguments.
CacheIdentity cache_identity_from_request(const std::string& request_text);

// Adds DownloadProxy query parameters observed on app segment requests while
// preserving values already present in the playlist URL. video_feature is
// vl.vi[].feature from the decoded getvinfo response.
std::string normalize_segment_url(std::string url, int video_feature = 0,
                                  int cdn_strategy = 0,
                                  int emergency_time_ms = -1);

// Mirrors libDownloadProxy.so GenMDSEPublicParams:
// source+0x14 = 1000 * trunc(Scheduler.remainSeconds_0x8A4 / 2).
int emergency_time_from_remain_seconds(int remain_seconds);

// Deterministic test hooks for native DRM TS codec detection. Production code
// uses the same PMT/Annex-B probes before decrypting selected VCL NAL units.
std::string detect_annexb_video_codec_for_test(const std::string& payload);
std::string detect_pmt_video_codec_for_test(const std::string& section);
bool response_has_anc_for_test(const std::string& response_text);
bool playlist_has_encryption_key_for_test(const std::string& playlist_text);
std::string strip_ad_segments_for_test(const std::string& playlist_text);
size_t subtitle_track_count_for_test(const nlohmann::json& decoded);
std::string merge_webvtt_segments_for_test(const std::vector<std::string>& segments);
std::string convert_subtitle_for_test(const std::string& webvtt,
                                      const std::string& format);

int run_download(const Options& opt);

// Decrypts an app getvinfo ANC envelope using the matching request cKey.
nlohmann::json decrypt_anc(const std::string& response_text,
                           const std::string& request_text,
                           bool trace = false,
                           const AncOptions& anc = AncOptions{});

}  // namespace tencent::download
