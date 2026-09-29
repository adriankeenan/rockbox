#!/bin/sh
# Generate H.264 baseline test clips plus ffmpeg reference checksums.
# Output goes to $1 (default: ./fixtures). Requires ffmpeg with libx264.
set -eu

OUT="${1:-$(dirname "$0")/fixtures}"
mkdir -p "$OUT"

command -v ffmpeg >/dev/null || { echo "ffmpeg not found" >&2; exit 1; }

# name  size  source            extra x264 params
gen() {
    name=$1; size=$2; src=$3; xp=$4
    # Raw Annex-B stream (decoder unit tests)
    ffmpeg -v error -y -f lavfi -i "${src}=size=${size}:rate=15" -t 4 \
        -c:v libx264 -profile:v baseline -pix_fmt yuv420p -x264-params "${xp}" \
        -bsf:v h264_mp4toannexb -f h264 "$OUT/$name.264"
    # Reference per-frame MD5 of the decoded (cropped) YUV420 frames
    ffmpeg -v error -y -i "$OUT/$name.264" -pix_fmt yuv420p -f framemd5 "$OUT/$name.md5"
    # MPEG-PS with MP2 audio (player end-to-end tests)
    ffmpeg -v error -y -f lavfi -i "${src}=size=${size}:rate=15" \
        -f lavfi -i "sine=frequency=440:sample_rate=44100" -t 4 \
        -c:v libx264 -profile:v baseline -pix_fmt yuv420p -x264-params "${xp}" \
        -c:a mp2 -b:a 128k -f vob "$OUT/$name.h264"
}

gen qvga_testsrc   320x240 testsrc2    "keyint=30:bframes=0:ref=1"
gen qvga_mandel    320x240 mandelbrot  "keyint=15:bframes=0:ref=3:slices=4"
gen qcif_testsrc   176x144 testsrc2    "keyint=30:bframes=0:ref=1"
# Non-multiple-of-16 height exercises the cropping path
gen crop_testsrc   320x180 testsrc2    "keyint=30:bframes=0:ref=2"

echo "fixtures written to $OUT"
