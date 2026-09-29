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
    # MPEG-TS with MP2 audio (transport stream tests)
    ffmpeg -v error -y -f lavfi -i "${src}=size=${size}:rate=15" \
        -f lavfi -i "sine=frequency=440:sample_rate=44100" -t 4 \
        -c:v libx264 -profile:v baseline -pix_fmt yuv420p -x264-params "${xp}" \
        -c:a mp2 -b:a 128k -f mpegts "$OUT/$name.ts"
    ffmpeg -v error -y -i "$OUT/$name.ts" -map 0:v -pix_fmt yuv420p -f framemd5 "$OUT/$name.ts.md5"
    # Reference PCM of the (mono) MP2 audio, duplicated to stereo
    for ext in h264 ts; do
        ffmpeg -v error -y -i "$OUT/$name.$ext" -map 0:a -af "pan=stereo|c0=c0|c1=c0" \
            -f s16le -ar 44100 "$OUT/$name.$ext.pcm"
    done
}

gen qvga_testsrc   320x240 testsrc2    "keyint=30:bframes=0:ref=1"
gen qvga_mandel    320x240 mandelbrot  "keyint=15:bframes=0:ref=3:slices=4"
gen qcif_testsrc   176x144 testsrc2    "keyint=30:bframes=0:ref=1"
# Non-multiple-of-16 height exercises the cropping path
gen crop_testsrc   320x180 testsrc2    "keyint=30:bframes=0:ref=2"



# AAC (ADTS in MPEG-TS) with reference PCM, for the audio path. The mono
# reference duplicates the channel: ffmpeg's plain -ac 2 upmix would scale by
# -3 dB, which is not what the player (or libfaad) does.
gen_aac() {
    name=$1; ch=$2; br=$3
    ffmpeg -v error -y -f lavfi -i "testsrc2=size=320x240:rate=24" \
        -f lavfi -i "aevalsrc=0.30*sin(2*PI*440*t)+0.20*sin(2*PI*1234*t)*sin(2*PI*3*t)+0.15*sin(2*PI*5200*t)*sin(2*PI*0.7*t)+0.05*(random(0)-0.5):s=44100:c=$ch" \
        -t 10 -c:v libx264 -profile:v baseline -level 2.1 -pix_fmt yuv420p \
        -x264-params "ref=3:bframes=0:keyint=48:scenecut=0:bitrate=400" \
        -c:a aac -b:a "$br" -f mpegts "$OUT/$name.ts"
    ffmpeg -v error -y -i "$OUT/$name.ts" -map 0:v -pix_fmt yuv420p -f framemd5 "$OUT/$name.ts.md5"
    if [ "$ch" = mono ]; then af="pan=stereo|c0=c0|c1=c0"; else af="anull"; fi
    ffmpeg -v error -y -i "$OUT/$name.ts" -map 0:a -af "$af" -f s16le -ar 44100 "$OUT/$name.ts.pcm"
}
gen_aac aac_stereo stereo 96k
gen_aac aac_mono mono 64k
# Pure tone: an unambiguous check of level, frequency and continuity
ffmpeg -v error -y -f lavfi -i "testsrc2=size=320x240:rate=24" -f lavfi -i "sine=frequency=1000:sample_rate=44100" -t 6 \
    -c:v libx264 -profile:v baseline -level 2.1 -pix_fmt yuv420p -x264-params "ref=3:bframes=0:keyint=48:scenecut=0:bitrate=300" \
    -c:a aac -b:a 96k -ac 2 -f mpegts "$OUT/aac_sine.ts"
ffmpeg -v error -y -i "$OUT/aac_sine.ts" -map 0:v -pix_fmt yuv420p -f framemd5 "$OUT/aac_sine.ts.md5"
ffmpeg -v error -y -i "$OUT/aac_sine.ts" -map 0:a -f s16le -ac 2 -ar 44100 "$OUT/aac_sine.ts.pcm"
echo "fixtures written to $OUT"
