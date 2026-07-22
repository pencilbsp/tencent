# libs/core — tencent::core
#
# Toàn bộ nghiệp vụ nằm ở đây và KHÔNG biết gì về CLI: không đọc argv, không in
# ra stdout, không std::exit. Nhờ vậy bản GUI sau này chỉ cần link đúng thư viện
# này (xem docs/ARCHITECTURE.md).
#
# Thứ tự các nhóm dưới đây phản ánh tầng phụ thuộc, từ dưới lên:
#   common → crypto/codec → net → model → protocol → storage/device → download → service

add_library(tencent_core STATIC
  # ── common: từ vựng dùng chung (không phụ thuộc gì ngoài std + nlohmann) ────
  src/common/ordered_kv.cpp
  src/common/hex.cpp
  src/common/strings.cpp
  src/common/clock.cpp
  src/common/json.cpp
  src/common/logging.cpp

  # ── crypto: primitive tùy biến của Tencent, PHẢI byte-exact ────────────────
  src/crypto/qqtea.cpp
  src/crypto/poly131.cpp
  src/crypto/tencent_base64.cpp
  src/crypto/xor_cipher.cpp
  src/crypto/md5.cpp
  src/crypto/rc4.cpp
  src/crypto/chacha20.cpp
  src/crypto/crc16.cpp
  src/crypto/bionic_random.cpp

  # ── codec: định dạng wire trung lập ────────────────────────────────────────
  src/codec/protobuf.cpp

  # ── net: transport ─────────────────────────────────────────────────────────
  src/net/http_client.cpp
  src/net/proxy.cpp

  # ── model: dữ liệu thuần (account, vật liệu thiết bị) ──────────────────────
  src/model/account.cpp
  src/model/session_material.cpp
  src/model/turing_material.cpp

  # ── protocol: dựng/parse request theo từng nghiệp vụ ───────────────────────
  src/protocol/trpc/head.cpp
  src/protocol/auth/login_sign.cpp
  src/protocol/auth/login_refresh.cpp
  src/protocol/auth/vip_info.cpp
  src/protocol/auth/user_center.cpp
  src/protocol/identity/turing_build.cpp
  src/protocol/identity/qimei_client.cpp
  src/protocol/identity/guid_client.cpp
  src/protocol/playback/constants.cpp
  src/protocol/playback/capability.cpp
  src/protocol/playback/request_context.cpp
  src/protocol/playback/ckey_codec.cpp
  src/protocol/playback/ckey_fields.cpp
  src/protocol/playback/adpass.cpp
  src/protocol/playback/request_builder.cpp

  # ── storage: bố cục file trạng thái trên đĩa ───────────────────────────────
  src/storage/paths.cpp
  src/storage/account_store.cpp

  # ── device: nói chuyện với thiết bị Android ────────────────────────────────
  src/device/adb_bridge.cpp
  src/device/helper_probe.cpp

  # ── download: tải/giải mã/remux ────────────────────────────────────────────
  src/download/proxy_config.cpp
  src/download/pipeline.cpp

  # ── service: mặt tiền cho giao diện (CLI hôm nay, GUI ngày mai) ────────────
  src/service/error.cpp
  src/service/account_service.cpp
  src/service/playback_service.cpp
  src/service/download_service.cpp
  src/service/diagnostics_service.cpp
)
add_library(tencent::core ALIAS tencent_core)

target_include_directories(tencent_core PUBLIC include)

target_link_libraries(tencent_core
  PUBLIC
    tencent::platform            # tiến trình con, đường dẫn, socket
    tencent::third_party_headers # nlohmann/json (header-only)
    ZLIB::ZLIB                   # gzip + crc32
    CURL::libcurl                # HTTP
    tencent::openssl_crypto      # AES/HMAC cho proxy & qimei
  PRIVATE
    tencent::project_options
    tencent::ffmpeg              # chỉ pipeline dùng; không rò ra ngoài
)

if(TENCENT_CLI_STANDALONE)
  target_compile_definitions(tencent_core PUBLIC TENCENT_CLI_STANDALONE=1)
endif()
