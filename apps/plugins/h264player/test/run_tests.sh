#!/bin/sh
# Host decoder tests: libh264bsd output must match ffmpeg frame-for-frame.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
LIB="$HERE/../libh264bsd"
FIX="$HERE/fixtures"
BLD="${BUILD_DIR:-$HERE/build}"
mkdir -p "$BLD"
fail=0

[ -d "$FIX" ] || "$HERE/gen_fixtures.sh" "$FIX" || exit 1

build() { # name cc flags
    name=$1; cc=$2; shift 2
    $cc -std=gnu11 -O2 -I"$LIB" "$@" -o "$BLD/dec_$name" \
        "$HERE/h264dec_test.c" "$LIB"/h264bsd_*.c 2>"$BLD/build_$name.log" \
        || { echo "FAIL build $name (see $BLD/build_$name.log)"; return 1; }
}

md5s() { # yuv width height -> per-frame md5 list
    python3 - "$1" "$2" "$3" <<'P'
import sys, hashlib
f, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
sz = w*h*3//2
d = open(f, 'rb').read()
for i in range(0, len(d) - sz + 1, sz):
    print(hashlib.md5(d[i:i+sz]).hexdigest())
P
}

check() { # variant runner clip
    variant=$1; runner=$2; clip=$3
    ref="$FIX/$clip.md5"
    dims=$(ffprobe -v error -select_streams v -show_entries stream=width,height \
           -of csv=p=0 "$FIX/$clip.264" | tr , ' ')
    set -- $dims
    $runner "$BLD/dec_$variant" "$FIX/$clip.264" "$BLD/$variant.$clip.yuv" 2>"$BLD/$variant.$clip.err" \
        || { echo "FAIL $variant $clip: decoder exited non-zero"; fail=1; return; }
    md5s "$BLD/$variant.$clip.yuv" "$1" "$2" > "$BLD/$variant.$clip.got"
    grep -v '^#' "$ref" | awk -F, '{gsub(/ /,"",$6); print $6}' > "$BLD/$clip.want"
    if cmp -s "$BLD/$variant.$clip.got" "$BLD/$clip.want"; then
        echo "PASS $variant $clip ($(wc -l < "$BLD/$clip.want") frames)"
    else
        echo "FAIL $variant $clip: frame MD5s differ from ffmpeg"; fail=1
    fi
}

CLIPS="qvga_testsrc qvga_mandel qcif_testsrc crop_testsrc"

build native gcc && for c in $CLIPS; do check native "" $c; done
build asan gcc -g -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=undefined \
    && for c in $CLIPS; do check asan "" $c; done
build m32 gcc -m32 && for c in $CLIPS; do check m32 "" $c; done
if command -v mipsel-linux-gnu-gcc >/dev/null && command -v qemu-mipsel >/dev/null; then
    build mips mipsel-linux-gnu-gcc -static -mno-unaligned-access 2>/dev/null \
        || build mips mipsel-linux-gnu-gcc -static
    for c in $CLIPS; do check mips "qemu-mipsel" $c; done
else
    echo "SKIP mips (mipsel-linux-gnu-gcc / qemu-mipsel missing)"
fi


# Shim (h264dec.c) fed PES payloads in random chunk sizes: output must equal a
# direct decode of the raw stream, every frame including the last.
gcc -std=gnu11 -O2 -g -DH264BSD_EXTERNAL_ALLOC -fsanitize=address,undefined \
    -fno-sanitize=shift-base -I"$HERE/.." -I"$LIB" -o "$BLD/shim_asan" \
    "$HERE/shim_test.c" "$HERE/../h264dec.c" "$LIB"/h264bsd_*.c 2>"$BLD/build_shim.log" \
    || { echo "FAIL build shim (see $BLD/build_shim.log)"; fail=1; }
if [ -x "$BLD/shim_asan" ]; then
    for c in $CLIPS; do
        for seed in 1 2 3; do
            "$BLD/shim_asan" "$FIX/$c.h264" "$BLD/shim.$c.yuv" $seed 2>"$BLD/shim.$c.err" \
                || { echo "FAIL shim $c seed $seed: crashed"; tail -3 "$BLD/shim.$c.err"; fail=1; continue; }
            md5s "$BLD/shim.$c.yuv" $(ffprobe -v error -select_streams v -show_entries stream=width,height -of csv=p=0 "$FIX/$c.264" | tr , ' ') > "$BLD/shim.$c.got"
            if cmp -s "$BLD/shim.$c.got" "$BLD/$c.want"; then
                echo "PASS shim $c seed $seed"
            else
                echo "FAIL shim $c seed $seed: frames differ from ffmpeg ($(wc -l < "$BLD/shim.$c.got") vs $(wc -l < "$BLD/$c.want"))"; fail=1
            fi
        done
    done
fi

# Robustness: truncated and corrupted streams must not crash under ASan.
for c in qvga_testsrc qvga_mandel; do
    sz=$(wc -c < "$FIX/$c.264")
    head -c $((sz / 2)) "$FIX/$c.264" > "$BLD/trunc.264"
    python3 - "$FIX/$c.264" "$BLD/corrupt.264" <<'P'
import sys, random
d = bytearray(open(sys.argv[1], 'rb').read()); random.seed(1)
for _ in range(200): d[random.randrange(64, len(d))] ^= 1 << random.randrange(8)
open(sys.argv[2], 'wb').write(d)
P
    for s in trunc corrupt; do
        if "$BLD/dec_asan" "$BLD/$s.264" /dev/null 2>"$BLD/$s.err"; then
            echo "PASS robustness $s $c"
        else
            echo "FAIL robustness $s $c"; tail -3 "$BLD/$s.err"; fail=1
        fi
    done
done

[ $fail = 0 ] && echo "ALL PASS" || echo "SOME FAILED"
exit $fail
