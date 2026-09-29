/* Host test driver: decode an Annex-B H.264 stream with libh264bsd and write
 * the cropped I420 frames to stdout (or to a file given as argv[2]). */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "h264bsd_decoder.h"

static void emit(storage_t *st, const uint8_t *pic, FILE *out)
{
    u32 flag = 0, left = 0, cw = 0, top = 0, ch = 0;
    u32 w = h264bsdPicWidth(st) * 16, h = h264bsdPicHeight(st) * 16;
    h264bsdCroppingParams(st, &flag, &left, &cw, &top, &ch);
    if (!flag) { left = top = 0; cw = w; ch = h; }

    for (u32 y = 0; y < ch; y++)
        fwrite(pic + (top + y) * w + left, 1, cw, out);
    const uint8_t *u = pic + w * h, *v = u + (w / 2) * (h / 2);
    for (int p = 0; p < 2; p++)
        for (u32 y = 0; y < ch / 2; y++)
            fwrite((p ? v : u) + (top / 2 + y) * (w / 2) + left / 2, 1, cw / 2, out);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s in.264 [out.yuv]\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n), *p = buf;
    if (fread(buf, 1, n, f) != (size_t)n) return 2;
    fclose(f);
    FILE *out = argc > 2 ? fopen(argv[2], "wb") : stdout;

    storage_t *st = h264bsdAlloc();
    if (!st || h264bsdInit(st, 1) != 0) { fprintf(stderr, "init failed\n"); return 1; }

    long left = n; int frames = 0, errs = 0;
    while (left > 0) {
        u32 used = 0, id, idr, nerr;
        u32 r = h264bsdDecode(st, p, left, 0, &used);
        p += used; left -= used;
        if (r == H264BSD_PIC_RDY) {
            uint8_t *pic;
            while ((pic = h264bsdNextOutputPicture(st, &id, &idr, &nerr))) {
                emit(st, pic, out); frames++;
            }
        } else if (r == H264BSD_ERROR || r == H264BSD_PARAM_SET_ERROR ||
                   r == H264BSD_MEMALLOC_ERROR) {
            errs++;
            if (!used) break;
        }
    }
    h264bsdFlushBuffer(st);
    {
        u32 id, idr, nerr; uint8_t *pic;
        while ((pic = h264bsdNextOutputPicture(st, &id, &idr, &nerr))) {
            emit(st, pic, out); frames++;
        }
    }
    h264bsdShutdown(st); h264bsdFree(st);
    if (out != stdout) fclose(out);
    free(buf);
    fprintf(stderr, "frames=%d errors=%d\n", frames, errs);
    return 0;
}
