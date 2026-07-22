# FFmpeg.cmake — Dựng libav* tĩnh từ source FFmpeg đã vendor (tag n8.1.1).
#
# Pipeline tải video link THẲNG vào libavformat/libavcodec/libavutil; dự án
# KHÔNG bao giờ spawn binary `ffmpeg`. Cấu hình --disable-everything rồi bật
# đúng những demuxer/decoder/muxer cần cho HLS→MP4 remux.
#
# TENCENT_FFMPEG_PREFIX: trỏ tới một prefix ĐÃ build sẵn (chứa lib/libavformat.a
# và include/) để tái dùng, bỏ qua bước build ~10 phút. Rất hữu ích khi tạo lại
# thư mục build. Bỏ trống = tự build.

include_guard(GLOBAL)
include(ExternalProject)

set(TENCENT_FFMPEG_PREFIX "" CACHE PATH
  "Prefix FFmpeg đã build sẵn (bỏ trống để tự build từ third_party/ffmpeg)")

if(TENCENT_FFMPEG_PREFIX)
  # ── Tái dùng bản đã build ──────────────────────────────────────────────────
  set(_ffmpeg_prefix "${TENCENT_FFMPEG_PREFIX}")
  set(_ffmpeg_needs_build FALSE)
else()
  # ── Tự build từ source đã vendor ───────────────────────────────────────────
  set(_ffmpeg_source "${CMAKE_SOURCE_DIR}/third_party/ffmpeg")
  if(NOT EXISTS "${_ffmpeg_source}/configure")
    message(FATAL_ERROR
      "Thiếu third_party/ffmpeg. Clone tag n8.1.1 từ https://git.ffmpeg.org/ffmpeg.git")
  endif()
  set(_ffmpeg_prefix "${CMAKE_BINARY_DIR}/ffmpeg-lib")
  set(_ffmpeg_needs_build TRUE)
endif()

set(_ffmpeg_avformat "${_ffmpeg_prefix}/lib/libavformat.a")
set(_ffmpeg_avcodec "${_ffmpeg_prefix}/lib/libavcodec.a")
set(_ffmpeg_avutil "${_ffmpeg_prefix}/lib/libavutil.a")
file(MAKE_DIRECTORY "${_ffmpeg_prefix}/include")

if(_ffmpeg_needs_build)
  find_program(MAKE_EXECUTABLE make REQUIRED)
  set(_ffmpeg_deploy_args)
  if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND _ffmpeg_deploy_args
      "--extra-cflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
      "--extra-ldflags=-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif()

  ExternalProject_Add(ffmpeg_external
    SOURCE_DIR "${_ffmpeg_source}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/ffmpeg-lib-build"
    CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E env
      "MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
      "${_ffmpeg_source}/configure" "--prefix=${_ffmpeg_prefix}"
      --disable-autodetect --disable-debug --disable-doc --disable-network
      --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample
      --disable-programs --disable-x86asm --disable-everything
      ${_ffmpeg_deploy_args}
      --enable-protocol=file
      --enable-demuxer=mpegts,mov,hevc,h264,aac
      --enable-muxer=mp4,mov
      --enable-decoder=aac,h264,hevc,vvc
      --enable-parser=aac,h264,hevc,vvc
      --enable-bsf=aac_adtstoasc,extract_extradata
      --enable-static --disable-shared
    BUILD_COMMAND "${MAKE_EXECUTABLE}" -j4
    INSTALL_COMMAND "${MAKE_EXECUTABLE}" install
    BUILD_BYPRODUCTS "${_ffmpeg_avformat}" "${_ffmpeg_avcodec}" "${_ffmpeg_avutil}"
    UPDATE_COMMAND "")
endif()

# ── Imported target: avutil ← avcodec ← avformat ─────────────────────────────
function(_tencent_add_ffmpeg_lib name path)
  add_library(${name} STATIC IMPORTED GLOBAL)
  set_target_properties(${name} PROPERTIES
    IMPORTED_LOCATION "${path}"
    INTERFACE_INCLUDE_DIRECTORIES "${_ffmpeg_prefix}/include")
  if(_ffmpeg_needs_build)
    add_dependencies(${name} ffmpeg_external)
  endif()
endfunction()

_tencent_add_ffmpeg_lib(ffmpeg_avutil "${_ffmpeg_avutil}")
target_link_libraries(ffmpeg_avutil INTERFACE Threads::Threads)
if(APPLE)
  # libavutil dùng VideoToolbox/CoreMedia cho các kiểu pixel phần cứng.
  target_link_libraries(ffmpeg_avutil INTERFACE
    "-framework CoreFoundation" "-framework CoreVideo" "-framework CoreMedia")
endif()

_tencent_add_ffmpeg_lib(ffmpeg_avcodec "${_ffmpeg_avcodec}")
target_link_libraries(ffmpeg_avcodec INTERFACE ffmpeg_avutil Threads::Threads)

_tencent_add_ffmpeg_lib(ffmpeg_avformat "${_ffmpeg_avformat}")
target_link_libraries(ffmpeg_avformat INTERFACE ffmpeg_avcodec ffmpeg_avutil)

add_library(tencent_ffmpeg INTERFACE)
target_link_libraries(tencent_ffmpeg INTERFACE ffmpeg_avformat ffmpeg_avcodec ffmpeg_avutil)
add_library(tencent::ffmpeg ALIAS tencent_ffmpeg)
