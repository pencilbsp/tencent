#!/bin/bash
# Download 96 episodes from Tencent Video
# Cover ID: mzc00200aaogpgh

set -e

OUTPUT_DIR=~/Downloads/DLTNC/SS2
mkdir -p "$OUTPUT_DIR"

declare -a VIDS=(
  "j0044a8qnhv"
  "o00446kxeo1"
  "y0044iv2muw"
  "x0044dubct3"
  "j00459tikds"
  "s0045h0fngg"
  "v00454gk2f5"
  "c004522ctj8"
  "w0045lw6hfv"
  "i0045mck208"
  "x0045p9o2p7"
  "l0045oaysyi"
)

TOTAL=${#VIDS[@]}
echo "=========================================="
echo " Bắt đầu tải $TOTAL tập"
echo " Output: $OUTPUT_DIR"
echo "=========================================="

for i in "${!VIDS[@]}"; do
  EP=$((i + 167))
  EP_PAD=$(printf "%02d" $EP)
  VID="${VIDS[$i]}"

  echo ""
  echo "[Tập $EP_PAD/$TOTAL] VID: $VID"
  echo "------------------------------------------"

  tencent-cli getvinfo \
    --url "https://v.qq.com/x/cover/mzc002007od0otb/${VID}.html" \
    --account "3578420805" \
    --enable-drm \
    --timeout 60 \
    --retries 3 \
    --getplay-sleep 5 \
    --workers 6 \
    --no-cache \
    --defn "suhd" \
    -o "$OUTPUT_DIR/SS2DLTNC01${EP_PAD}_4k.mp4"

  echo "[Tập $EP_PAD] ✅ Hoàn thành"
done

echo ""
echo "=========================================="
echo " ✅ Đã tải xong tất cả $TOTAL tập!"
echo "=========================================="
