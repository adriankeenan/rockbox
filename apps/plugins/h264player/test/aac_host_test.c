/* Host driver for aac_dec.c: decode a raw ADTS file to s16le stereo. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "aac_dec.h"

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb"), *o = fopen(argv[2], "wb");
    long n; uint8_t *buf; long pos = 0; unsigned frames = 0, errs = 0;
    static uint8_t heap[512 * 1024];
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(n + 16); memset(buf, 0, n + 16);
    if (fread(buf, 1, n, f) != (size_t)n) return 2;

    if (!aac_open(heap, sizeof heap)) { fprintf(stderr, "open failed\n"); return 1; }
    int inited = 0;
    while (pos < n) {
        unsigned flen, rate, ch, samples;
        struct aac_info info;
        if (adts_parse(buf + pos, n - pos, &flen, &rate, &ch, &samples)) { pos++; continue; }
        if (pos + flen > n) break;
        if (!inited) { if (!aac_init(buf + pos, flen)) { fprintf(stderr, "init failed\n"); return 1; } inited = 1; }
        int r = aac_decode(buf + pos, flen, &info);
        pos += flen;
        if (r < 0) { errs++; continue; }
        for (unsigned i = 0; i < info.samples; i++) {
            for (int c = 0; c < 2; c++) {
                int64_t v = info.pcm[c < (int)info.channels ? c : 0][i];
                v = (v + (1 << 13)) >> 14;                 /* 29 -> 15 fractional bits */
                if (v > 32767) v = 32767; if (v < -32768) v = -32768;
                int16_t s = (int16_t)v; fwrite(&s, 2, 1, o);
            }
        }
        frames++;
    }
    fprintf(stderr, "frames=%u errors=%u heap_used=%zu rate=%u\n", frames, errs, aac_heap_used(), 0u);
    fclose(o); return 0;
}
