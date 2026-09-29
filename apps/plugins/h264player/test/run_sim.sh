#!/bin/sh
# Headless simulator test of the whole plugin.
#   run_sim.sh [target] [builddir]      (default target: erosqnative)
# Builds the simulator, plays each fixture through h264player with pacing off,
# and checks every drawn frame's MD5 against ffmpeg, then checks the frames
# after a mid-stream seek are a contiguous tail of the reference.
set -u
TARGET=${1:-erosqnative}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../../.." && pwd)
BLD=${2:-${SIM_BUILD_DIR:-$HERE/build/sim_$TARGET}}
FIX="$HERE/fixtures"
CLIPS=${CLIPS:-"qvga_testsrc qvga_mandel qcif_testsrc crop_testsrc"}
fail=0

[ -d "$FIX" ] || "$HERE/gen_fixtures.sh" "$FIX" || exit 1
mkdir -p "$BLD"
if [ ! -f "$BLD/Makefile" ]; then
    (cd "$BLD" && printf '\n' | "$ROOT/tools/configure" --target="$TARGET" --type=s >configure.log 2>&1) \
        || { echo "FAIL configure $TARGET (see $BLD/configure.log)"; exit 1; }
fi
(cd "$BLD" && make -j"$(nproc)" >make.log 2>&1 && make install >install.log 2>&1) \
    || { echo "FAIL build $TARGET (see $BLD/make.log)"; exit 1; }
echo "PASS build $TARGET"

export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
export ROCKBOX_AUTORUN_PLUGIN=/.rockbox/rocks/viewers/h264player.rock
export ROCKBOX_AUTORUN_PARAM=/test.h264
DISK="$BLD/simdisk"

for c in $CLIPS; do
    rm -f "$DISK"/h264player_test.log "$DISK"/h264player_shot.bmp
    cp "$FIX/$c.h264" "$DISK/test.h264"
    : > "$DISK/h264player.test"
    (cd "$BLD" && timeout 300 ./rockboxui >run_$c.log 2>&1)
    rc=$?
    LOG="$DISK/h264player_test.log"
    if [ $rc -ne 0 ] || [ ! -s "$LOG" ]; then
        echo "FAIL sim $TARGET $c: rockboxui rc=$rc (see $BLD/run_$c.log)"; fail=1; continue
    fi
    python3 - "$LOG" "$FIX/$c.md5" "$TARGET $c" <<'P' || fail=1
import sys
log, ref, name = sys.argv[1], sys.argv[2], sys.argv[3]
passes, meta = [[]], {}
for l in open(log).read().split('\n'):
    if l.startswith('PHASE seek'): passes.append([])
    elif l.startswith('F '): passes[-1].append(l.split()[2])
    elif l.startswith('OPEN '): meta['open'] = int(l.split()[1])
    elif l.startswith('TIMEOUT'): meta['timeout'] = True
want = [l.split(',')[5].strip() for l in open(ref) if not l.startswith('#')]
p1 = passes[0]
ok = True
def bad(msg):
    global ok
    ok = False; print('FAIL sim %s: %s' % (name, msg))
if meta.get('open', -1) < 0: bad('open failed (%s)' % meta.get('open'))
elif meta.get('timeout'): bad('timed out')
elif p1 != want:
    d = next((i for i, (a, b) in enumerate(zip(p1, want)) if a != b), min(len(p1), len(want)))
    bad('play-through differs from ffmpeg: %d vs %d frames, first diff at %d' % (len(p1), len(want), d))
elif len(passes) < 2:
    bad('no seek passes ran')
else:
    starts = []
    for n, p2 in enumerate(passes[1:]):
        k = len(p2)
        st = [i for i in range(len(want) - k + 1) if want[i:i+k] == p2] if k else []
        if k == 0 or not st or st[0] + k != len(want):
            bad('seek %d: got %d frames, not a tail of the reference' % (n + 1, k))
            break
        starts.append(st[0])
    else:
        print('PASS sim %s: %d frames exact; seeks resumed at frames %s' % (name, len(p1), starts))
sys.exit(0 if ok else 1)
P
    [ -f "$DISK/h264player_shot.bmp" ] && cp "$DISK/h264player_shot.bmp" "$BLD/shot_$c.bmp"
done

[ $fail = 0 ] && echo "SIM ALL PASS ($TARGET)" || echo "SIM SOME FAILED ($TARGET)"
exit $fail
