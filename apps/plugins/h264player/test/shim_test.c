/* Host test for h264dec.c: reads an MPEG-PS file (as produced by
 * gen_fixtures.sh), feeds video PES payloads to the mpeg2_* shim in
 * randomly sized chunks with PTS tags, and writes cropped I420 frames.
 * Usage: shim_test in.h264 out.yuv [seed] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "h264dec.h"

/* Bump allocator standing in for the plugin's */
static unsigned char pool[4 << 20];
static unsigned pool_pos, pool_lock;
void *mpeg2_malloc(unsigned size, mpeg2_alloc_t r)
{
    void *p = pool + pool_pos; (void)r;
    size = (size + 3) & ~3u;
    if (pool_pos + size > sizeof pool) return NULL;
    pool_pos += size; memset(p, 0, size); return p;
}
void *mpeg2_bufalloc(unsigned size, mpeg2_alloc_t r)
{ void *p = mpeg2_malloc(size, r); pool_lock = pool_pos; return p; }
void mpeg2_mem_reset(void) { pool_pos = pool_lock; }

struct packet { const uint8_t *d; uint32_t n; int has_pts; uint32_t pts; };

/* Walk the PS and collect video (0xE0) PES payloads */
static int collect(const uint8_t *b, long n, struct packet *out, int max)
{
    int cnt = 0; long i = 0;
    while (i + 6 <= n) {
        if (b[i] || b[i+1] || b[i+2] != 1) { i++; continue; }
        uint8_t id = b[i+3];
        if (id == 0xBA) { i += 14 + (b[i+13] & 7); continue; }
        uint32_t len = (b[i+4] << 8) | b[i+5];
        if (id >= 0xE0 && id <= 0xEF) {
            const uint8_t *h = b + i + 6;
            int hl = h[2], pts_flag = h[1] >> 6;
            struct packet *p = &out[cnt++]; if (cnt > max) return -1;
            p->d = h + 3 + hl; p->n = len - 3 - hl; p->has_pts = pts_flag >= 2;
            if (p->has_pts) {
                const uint8_t *q = h + 3;
                uint64_t pts = ((uint64_t)((q[0] >> 1) & 7) << 30) | (q[1] << 22)
                    | ((q[2] >> 1) << 15) | (q[3] << 7) | (q[4] >> 1);
                p->pts = (uint32_t)pts;
            }
        }
        i += 6 + len;
    }
    return cnt;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    srand(argc > 3 ? atoi(argv[3]) : 1);
    FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n); if (fread(buf, 1, n, f) != (size_t)n) return 2; fclose(f);
    static struct packet pk[100000];
    int np = collect(buf, n, pk, 100000);
    FILE *out = fopen(argv[2], "wb");
    fprintf(stderr, "packets=%d\n", np);

    mpeg2dec_t *d = mpeg2_init(); if (!d) { fprintf(stderr, "init\n"); return 1; }
    const mpeg2_info_t *info = mpeg2_info(d);
    int frames = 0, seqs = 0, pics = 0, tagged = 0, i = 0;
    uint32_t off = 0, last_tag = 0; int ordered = 1;
    mpeg2_skip(d, 1);
    while (1) {
        switch (mpeg2_parse(d)) {
        case STATE_BUFFER: {
            if (i >= np) { if (mpeg2_end(d)) break; goto done; }
            /* Random sub-chunking of each PES payload to stress reassembly */
            uint32_t rem = pk[i].n - off;
            uint32_t c = (rand() % 3 == 0) ? rem : 1 + rand() % (rem < 700 ? rem : 700);
            if (off == 0 && pk[i].has_pts) mpeg2_tag_picture(d, pk[i].pts, 0);
            mpeg2_buffer(d, pk[i].d + off, pk[i].d + off + c);
            off += c; if (off >= pk[i].n) { i++; off = 0; }
            break; }
        case STATE_SEQUENCE: seqs++; break;
        case STATE_PICTURE:
            pics++;
            if (info->current_picture->flags & PIC_MASK_CODING_TYPE)
                mpeg2_skip(d, 0);
            break;
        case STATE_SLICE: {
            const mpeg2_sequence_t *s = info->sequence;
            uint32_t w = s->width, cw = s->display_width, ch = s->display_height;
            for (uint32_t y = 0; y < ch; y++) fwrite(info->display_fbuf->buf[0] + y * w, 1, cw, out);
            for (int p = 1; p < 3; p++)
                for (uint32_t y = 0; y < ch / 2; y++)
                    fwrite(info->display_fbuf->buf[p] + y * (w / 2), 1, cw / 2, out);
            if (info->display_picture->flags & PIC_FLAG_TAGS) {
                if (tagged && info->display_picture->tag < last_tag) ordered = 0;
                last_tag = info->display_picture->tag; tagged++;
            }
            frames++; break; }
        default: break;
        }
    }
done:
    mpeg2_close(d); fclose(out);
    fprintf(stderr, "frames=%d pictures=%d sequences=%d tagged=%d ordered_pts=%d period=%u\n",
            frames, pics, seqs, tagged, ordered, info->sequence ? info->sequence->frame_period : 0);
    return 0;
}
