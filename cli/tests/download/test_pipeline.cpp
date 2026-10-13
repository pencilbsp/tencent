// test_download_pipeline.cpp - Deterministic cache namespace tests.
// This covers local path/form parsing only; network and media decrypt remain
// live-verification concerns documented in AGENTS.md.
#include <cstdio>
#include <filesystem>
#include <string>

#include "tencent/core/download/pipeline.h"
#include "tencent/core/common/json.h"

namespace download = tencent::download;

static int g_failures = 0;

static void check(bool ok, const char* name) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) ++g_failures;
}

static std::string bytes(std::initializer_list<unsigned char> values) {
  std::string out;
  out.reserve(values.size());
  for (unsigned char v : values) out.push_back(static_cast<char>(v));
  return out;
}

static std::string annexb(std::initializer_list<std::string> nals) {
  std::string out;
  for (const auto& nal : nals) {
    out += bytes({0x00, 0x00, 0x00, 0x01});
    out += nal;
  }
  return out;
}

static std::string pmt_with_video(unsigned char stream_type) {
  // Minimal ISO/IEC 13818-1 PMT section: fixed program header, one elementary
  // stream descriptor, and placeholder CRC. Stream types under test are
  // 0x1b=AVC, 0x24=HEVC, 0x33=VVC.
  return bytes({0x02, 0xb0, 0x12, 0x00, 0x01, 0xc1, 0x00, 0x00, 0xe1, 0x00,
                0xf0, 0x00, stream_type, 0xe1, 0x01, 0xf0, 0x00, 0x00, 0x00,
                0x00, 0x00});
}

int main() {
  std::printf("== download_pipeline cache ==\n");

  std::filesystem::path expected =
      std::filesystem::path("/tmp/tencent") / "videos" / "z4102vwf8dr" / "uhd";
  check(download::cache_work_dir("/tmp/tencent", "z4102vwf8dr", "UHD") ==
            expected.string(),
        "cache is namespaced by VID and normalized quality");

  std::string request =
      "POST https://vv6.video.qq.com/getvinfo\n"
      "Content-Type: application/x-www-form-urlencoded\n\n"
      "cid=mzc0020027yzd9e&vid=z4102vwf8dr&defn=hdr10%2B60fps&scene=x\n";
  auto identity = download::cache_identity_from_request(request);
  check(identity.vid == "z4102vwf8dr", "parse VID from serialized request");
  check(identity.defn == "hdr10+60fps", "decode quality from serialized request");
  check(download::cache_work_dir("/tmp/tencent", identity.vid, identity.defn) ==
            (std::filesystem::path("/tmp/tencent") / "videos" / "z4102vwf8dr" /
             "hdr10_60fps")
                .string(),
        "unsafe path characters are normalized");

  bool traversal_rejected = false;
  try {
    (void)download::cache_work_dir("/tmp/tencent", "..", "uhd");
  } catch (const std::runtime_error&) {
    traversal_rejected = true;
  }
  check(traversal_rejected, "path traversal component is rejected");

  std::string normalized =
      download::normalize_segment_url("http://example.com/video/0.ts?ver=4&token=abc");
  check(normalized ==
            "http://example.com/video/0.ts?ver=4&token=abc&cost=low&"
            "cdn_strategy=0&dlipscene=0",
        "feature without bit 2 produces exact default segment query");

  std::string cdn_feature_url = download::normalize_segment_url(
      "http://example.com/video/0.ts?ver=4&token=abc", 4, 2);
  check(cdn_feature_url ==
            "http://example.com/video/0.ts?ver=4&token=abc&cost=low&"
            "cdn_strategy=2&dlipscene=10",
        "config CDNStrategy and feature produce exact segment query");

  check(download::emergency_time_from_remain_seconds(0) == 0,
        "remainSeconds 0 produces emergencyTime 0");
  check(download::emergency_time_from_remain_seconds(12) == 6000,
        "remainSeconds 12 produces emergencyTime 6000");

  std::string emergency_url = download::normalize_segment_url(
      "http://example.com/video/0.ts?ver=4", 0, 0,
      download::emergency_time_from_remain_seconds(0));
  check(emergency_url ==
            "http://example.com/video/0.ts?ver=4&cost=low&"
            "emergency_level=1&cdn_strategy=0&dlipscene=0",
        "emergencyTime <= 1000 appends emergency_level=1");

  std::string non_emergency_url = download::normalize_segment_url(
      "http://example.com/video/0.ts?ver=4", 0, 0,
      download::emergency_time_from_remain_seconds(12));
  check(non_emergency_url ==
            "http://example.com/video/0.ts?ver=4&cost=low&"
            "cdn_strategy=0&dlipscene=0",
        "emergencyTime > 1000 does not append emergency_level");

  std::string app_url =
      "http://example.com/video/0.ts?cost=high&cdn_strategy=2&dlipscene=10";
  check(download::normalize_segment_url(app_url, 0) == app_url,
        "segment URL keeps existing app query values");

  std::string playlist_url = "http://example.com/video/0.ts?ver=4&token=abc";
  check(download::normalize_segment_url(playlist_url, 0) != playlist_url,
        "native segment query augmentation remains opt-in at call sites");

  std::printf("\n== download_pipeline response and playlist classification ==\n");

  check(download::response_has_anc_for_test(R"({"anc":"abc"})"),
        "top-level anc response is detected");
  check(download::response_has_anc_for_test(R"({"resData":{"anc":"abc"}})"),
        "nested resData.anc response is detected");
  check(!download::response_has_anc_for_test(R"({"vl":{"vi":[]}})"),
        "plaintext getvinfo response does not require ANC");

  std::string clear_playlist =
      "#EXTM3U\n"
      "#EXT-X-KEY:METHOD=NONE\n"
      "0000.ts\n";
  check(!download::playlist_has_encryption_key_for_test(clear_playlist),
        "METHOD=NONE playlist is treated as plaintext");
  std::string encrypted_playlist =
      "#EXTM3U\n"
      "#EXT-X-KEY:METHOD=AES-128,URI=\"key.bin\"\n"
      "0000.ts\n";
  check(download::playlist_has_encryption_key_for_test(encrypted_playlist),
        "non-NONE EXT-X-KEY playlist requires decrypt material");

  std::string playlist_with_preroll =
      "#EXTM3U\n"
      "#EXT-X-DISCONTINUITY\n"
      "#EXTINF:15.040,\n"
      "https://ad.example/00.ts?segmenttype=2&token=ad\n"
      "#EXT-X-DISCONTINUITY\n"
      "#EXTINF:12.000,\n"
      "https://video.example/01.ts?index=0&token=video\n";
  std::string program_playlist = download::strip_ad_segments_for_test(playlist_with_preroll);
  check(program_playlist.find("segmenttype=2") == std::string::npos &&
            program_playlist.find("01.ts?index=0") != std::string::npos,
        "HLS pre-roll ad segment is excluded before remux");

  std::printf("\n== download_pipeline subtitles ==\n");

  nlohmann::json subtitle_response = nlohmann::json::parse(R"({
    "sfl": {
      "cnt": 1,
      "fi": [
        {
          "id": 53802,
          "lang": "ZH-CN",
          "name": "中文",
          "url": "http://subtitle.tc.qq.com/video.f53802000.vtt.m3u8?ver=4",
          "urlList": {
            "ui": [
              {"url": "http://subtitle.tc.qq.com/video.f53802000.vtt.m3u8?ver=4"},
              {"url": "http://subtitlebk.apdcdn.tc.qq.com/video.f53802000.vtt.m3u8?ver=4"}
            ]
          }
        }
      ]
    }
  })");
  check(download::subtitle_track_count_for_test(subtitle_response) == 1,
        "sfl.fi subtitle track is detected");
  check(download::subtitle_track_count_for_test(nlohmann::json::object()) == 0,
        "missing sfl.fi yields no subtitle tracks");

  std::string merged_vtt = download::merge_webvtt_segments_for_test({
      "WEBVTT\nX-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:0\n\n"
      "00:00:01.000 --> 00:00:02.000\nA\n",
      "WEBVTT\nX-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:900000\n\n"
      "00:00:03.000 --> 00:00:04.000\nB\n"});
  check(merged_vtt.find("WEBVTT") == 0 &&
            merged_vtt.find("WEBVTT", 1) == std::string::npos,
        "merged WebVTT keeps a single header");
  check(merged_vtt.find("A") != std::string::npos &&
            merged_vtt.find("B") != std::string::npos,
        "merged WebVTT keeps cue payloads");
  std::string srt = download::convert_subtitle_for_test(merged_vtt, "srt");
  check(srt.find("00:00:01,000 --> 00:00:02,000") != std::string::npos &&
            srt.find("A") != std::string::npos,
        "WebVTT converts to SRT timecode");
  std::string ass = download::convert_subtitle_for_test(merged_vtt, "ass");
  check(ass.find("[Events]") != std::string::npos &&
            ass.find("Dialogue: 0,0:00:01.00,0:00:02.00") != std::string::npos,
        "WebVTT converts to ASS dialogue");

  std::printf("\n== download_pipeline drm codec detection ==\n");

  std::string h264 = annexb({bytes({0x67, 0x42, 0x00, 0x1f, 0xe5}),
                             bytes({0x68, 0xce, 0x06, 0xe2}),
                             bytes({0x65, 0x88, 0x84, 0x00})});
  check(download::detect_annexb_video_codec_for_test(h264) == "h264",
        "Annex-B detector accepts H.264/AVC VCL and parameter NALs");

  std::string h265 = annexb({bytes({0x40, 0x01, 0x0c, 0x01}),
                             bytes({0x42, 0x01, 0x01, 0x60}),
                             bytes({0x44, 0x01, 0xc0, 0xf1}),
                             bytes({0x26, 0x01, 0xaf, 0x09})});
  check(download::detect_annexb_video_codec_for_test(h265) == "h265",
        "Annex-B detector keeps existing H.265/HEVC path");

  std::string h266 = annexb({bytes({0x00, 0x71, 0x01, 0x04}),
                             bytes({0x00, 0x79, 0x12, 0x00}),
                             bytes({0x00, 0x81, 0x33, 0x00}),
                             bytes({0x00, 0x39, 0xff, 0x20})});
  check(download::detect_annexb_video_codec_for_test(h266) == "h266",
        "Annex-B detector accepts H.266/VVC VCL and parameter NALs");

  check(download::detect_pmt_video_codec_for_test(pmt_with_video(0x1b)) == "h264",
        "PMT stream_type 0x1b selects H.264/AVC");
  check(download::detect_pmt_video_codec_for_test(pmt_with_video(0x24)) == "h265",
        "PMT stream_type 0x24 selects H.265/HEVC");
  check(download::detect_pmt_video_codec_for_test(pmt_with_video(0x33)) == "h266",
        "PMT stream_type 0x33 selects H.266/VVC");

  std::printf("\n%s (%d failure(s))\n",
              g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
  return g_failures == 0 ? 0 : 1;
}
