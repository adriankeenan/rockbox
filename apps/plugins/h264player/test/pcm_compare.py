#!/usr/bin/env python3
"""Compare the player's captured PCM (s16le stereo) with ffmpeg's decode.

usage: pcm_compare.py got.raw ref.raw [min_snr_db]
Finds the best sample lag (the two decoders may differ by a few samples of
priming), then reports SNR over the overlap. Exit 0 if SNR >= min_snr_db.
"""
import array, math, sys

def load(path):
    a = array.array('h')
    with open(path, 'rb') as f:
        data = f.read()
    a.frombytes(data[:len(data) // 4 * 4])
    return a

got, ref = load(sys.argv[1]), load(sys.argv[2])
need = float(sys.argv[3]) if len(sys.argv) > 3 else 35.0
gl, rl = got[0::2], ref[0::2]              # left channel for alignment

if len(gl) < 20000 or len(rl) < 20000:
    print('FAIL pcm: too short (got %d, ref %d samples)' % (len(gl), len(rl)))
    sys.exit(1)

W = 3000
start = min(len(rl), len(gl)) // 2
best = None
for lag in range(-4096, 4097):
    if start + lag < 0 or start + lag + W > len(gl):
        continue
    err = 0
    for i in range(0, W, 3):               # subsample for speed
        d = gl[start + lag + i] - rl[start + i]
        err += d * d
    if best is None or err < best[0]:
        best = (err, lag)
lag = best[1]

# Overlap of got shifted by lag against ref, skipping the codec priming
lo = max(0, -lag) + 4096
hi = min(len(rl), len(gl) - lag) - 4096
if hi <= lo:
    print('FAIL pcm: no overlap'); sys.exit(1)
sig = err = 0
for ch in (0, 1):
    g, r = got[ch::2], ref[ch::2]
    for i in range(lo, hi):
        a, b = r[i], g[i + lag]
        sig += a * a
        err += (a - b) * (a - b)
snr = 10 * math.log10(sig / err) if err else 99.0
ok = snr >= need
print('%s pcm: lag %d samples, SNR %.1f dB (need %.0f), got %d ref %d samples' %
      ('PASS' if ok else 'FAIL', lag, snr, need, len(gl), len(rl)))
sys.exit(0 if ok else 1)
