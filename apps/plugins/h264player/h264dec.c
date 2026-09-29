/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * libmpeg2-style front end over libh264bsd. See h264dec.h.
 *
 * Input arrives in arbitrary chunks (PES payloads). NAL units are
 * reassembled in an accumulation buffer and only complete ones (terminated
 * by the next start code) are handed to the decoder, since h264bsd cannot
 * resume a NAL split across calls.
 *
 * Output order equals decode order (baseline: no B-frames), so the tag of a
 * picture is simply carried through the decoder as its picture id.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <string.h>
#include "h264dec.h"
#include "libh264bsd/h264bsd_decoder.h"

#define ACC_SIZE      (128*1024)  /* largest NAL we can reassemble */
#define MAX_TAGS      8
#define DEFAULT_PERIOD 900900     /* 29.97 fps in 27MHz ticks */

#define NAL_SLICE     1
#define NAL_IDR       5
#define NAL_SPS       7

struct pend_tag { uint32_t pos, tag; };

struct mpeg2dec_s
{
    storage_t *st;
    mpeg2_info_t info;
    mpeg2_sequence_t seq;
    mpeg2_picture_t cur_pic, disp_pic;
    mpeg2_fbuf_t fbuf;
    int have_seq;

    uint8_t *acc;               /* accumulation buffer */
    uint32_t acc_len, acc_pos;  /* valid bytes; consumed up to */
    struct pend_tag tags[MAX_TAGS];
    int num_tags;

    /* Current NAL, located in acc */
    int have_nal;
    int announced;              /* STATE_PICTURE already returned for it */
    uint32_t nal_off, nal_len, nal_hdr;
    int nal_type;
    int nal_first_slice;

    int flushed;                /* mpeg2_end already appended its marker */
    int skip;                   /* requested via mpeg2_skip */
    int pic_skip;               /* latched at the first slice of a picture */
    int out_ready;              /* decoded picture waiting to be reported */
    uint32_t cur_flags, cur_tag;
};

/** Allocation hooks for libh264bsd **/

void *h264bsd_ext_malloc(size_t size)
{
    return mpeg2_malloc((unsigned)size, MPEG2_ALLOC_YUV);
}

void h264bsd_ext_free(void *ptr)
{
    (void)ptr; /* released in bulk by mpeg2_mem_reset */
}

/** Bit helpers **/

/* Exp-Golomb ue(v) read over emulation-prevention-free header bytes; only
 * used for the first few fields where 0x000003 is not expected to matter. */
static int read_ue(const uint8_t *p, uint32_t len, uint32_t *bitpos, uint32_t *val)
{
    uint32_t zeros = 0, v;
    while (1)
    {
        if ((*bitpos >> 3) >= len) return 0;
        if ((p[*bitpos >> 3] >> (7 - (*bitpos & 7))) & 1) break;
        if (++zeros > 31) return 0;
        (*bitpos)++;
    }
    (*bitpos)++;
    v = 1;
    while (zeros--)
    {
        if ((*bitpos >> 3) >= len) return 0;
        v = (v << 1) | ((p[*bitpos >> 3] >> (7 - (*bitpos & 7))) & 1);
        (*bitpos)++;
    }
    *val = v - 1;
    return 1;
}

/** NAL handling **/

static int find_start(const uint8_t *b, uint32_t from, uint32_t len)
{
    /* returns offset of the 00 00 01 triplet or -1 */
    uint32_t i;
    for (i = from; i + 2 < len; i++)
        if (b[i + 2] > 1)
            i += 2;
        else if (b[i] == 0 && b[i + 1] == 0 && b[i + 2] == 1)
            return (int)i;
    return -1;
}

static void compact(mpeg2dec_t *d)
{
    uint32_t n = d->acc_pos, i;
    if (n == 0) return;
    memmove(d->acc, d->acc + n, d->acc_len - n);
    d->acc_len -= n;
    d->acc_pos = 0;
    for (i = 0; i < (uint32_t)d->num_tags; i++)
        d->tags[i].pos = d->tags[i].pos > n ? d->tags[i].pos - n : 0;
    if (d->have_nal)
    {
        d->nal_off -= n;
        d->nal_hdr -= n;
    }
}

/* Locate the next complete NAL at/after acc_pos. */
static int extract_nal(mpeg2dec_t *d)
{
    int s1 = find_start(d->acc, d->acc_pos, d->acc_len);
    int s2;
    uint32_t hdr, end;

    if (s1 < 0)
    {
        /* Keep the last two bytes; a start code may straddle chunks */
        if (d->acc_len - d->acc_pos > 2)
            d->acc_pos = d->acc_len - 2;
        return 0;
    }

    hdr = (uint32_t)s1 + 3;
    s2 = find_start(d->acc, hdr, d->acc_len);
    if (s2 < 0)
    {
        d->acc_pos = (uint32_t)s1; /* wait for the terminating start code */
        return 0;
    }

    end = (uint32_t)s2;
    while (end > hdr && d->acc[end - 1] == 0) /* trailing_zero_8bits */
        end--;

    d->nal_off = (uint32_t)s1;
    d->nal_hdr = hdr;
    d->nal_len = end - (uint32_t)s1;
    d->acc_pos = (uint32_t)s2;    /* next search starts at the next code */
    d->nal_type = end > hdr ? (d->acc[hdr] & 0x1f) : 0;
    d->nal_first_slice = 0;

    if ((d->nal_type == NAL_SLICE || d->nal_type == NAL_IDR) && end > hdr + 1)
    {
        uint32_t bp = 0, first;
        if (read_ue(d->acc + hdr + 1, end - hdr - 1, &bp, &first))
            d->nal_first_slice = first == 0;
    }
    return 1;
}

static int slice_is_idr(const mpeg2dec_t *d)
{
    return d->nal_type == NAL_IDR;
}

/* Take the newest tag that begins at or before this NAL and drop older ones */
static uint32_t take_tag(mpeg2dec_t *d)
{
    int i, found = -1;
    for (i = 0; i < d->num_tags; i++)
        if (d->tags[i].pos <= d->nal_hdr)
            found = i;
    if (found < 0)
        return H264DEC_NO_TAG;
    {
        uint32_t tag = d->tags[found].tag;
        int keep = d->num_tags - (found + 1);
        memmove(d->tags, d->tags + found + 1, keep * sizeof (d->tags[0]));
        d->num_tags = keep;
        return tag;
    }
}

static void consume_nal(mpeg2dec_t *d)
{
    d->have_nal = 0;
    d->announced = 0;
}

/** Sequence info **/

static int update_sequence(mpeg2dec_t *d, const seqParamSet_t *sps, int force)
{
    mpeg2_sequence_t s;
    u32 flag = 0, left = 0, cw = 0, top = 0, ch = 0;
    uint32_t w = sps->picWidthInMbs * 16, h = sps->picHeightInMbs * 16;

    if (sps->frameCroppingFlag)
    {
        left = 2 * sps->frameCropLeftOffset;
        top  = 2 * sps->frameCropTopOffset;
        cw = w - left - 2 * sps->frameCropRightOffset;
        ch = h - top - 2 * sps->frameCropBottomOffset;
        flag = 1;
    }
    if (!flag || cw == 0 || ch == 0 || left + cw > w || top + ch > h)
    {
        left = top = 0; cw = w; ch = h;
    }

    memset(&s, 0, sizeof s);
    s.width = w;
    s.height = h;
    s.chroma_width = w / 2;
    s.chroma_height = h / 2;
    s.display_width = cw;
    s.display_height = ch;
    s.frame_period = DEFAULT_PERIOD;

    if (sps->vuiParametersPresentFlag && sps->vuiParameters &&
        sps->vuiParameters->timingInfoPresentFlag &&
        sps->vuiParameters->timeScale && sps->vuiParameters->numUnitsInTick)
    {
        uint64_t p = 27000000ull * 2 * sps->vuiParameters->numUnitsInTick;
        p /= sps->vuiParameters->timeScale;
        if (p >= 27000000ull / 120 && p <= 27000000ull)
            s.frame_period = (unsigned)p;
    }

    if (!force && d->have_seq && memcmp(&s, &d->seq, sizeof s) == 0)
        return 0;

    d->seq = s;
    d->have_seq = 1;
    d->info.sequence = &d->seq;
    return 1;
}

/* Crop offsets are re-derived at picture time so pointers can be offset */
static void crop_offsets(mpeg2dec_t *d, uint32_t *left, uint32_t *top)
{
    u32 flag = 0, l = 0, w = 0, t = 0, h = 0;
    h264bsdCroppingParams(d->st, &flag, &l, &w, &t, &h);
    if (!flag || l + w > d->seq.width || t + h > d->seq.height)
        l = t = 0;
    *left = l;
    *top = t;
}

/** Decoder lifetime **/

static int decoder_start(mpeg2dec_t *d)
{
    d->st = h264bsdAlloc();
    if (d->st == NULL || h264bsdInit(d->st, 1) != 0)
    {
        d->st = NULL;
        return 0;
    }
    return 1;
}

mpeg2dec_t * mpeg2_init(void)
{
    mpeg2dec_t *d = mpeg2_bufalloc(sizeof (*d), MPEG2_ALLOC_MPEG2DEC);
    if (d == NULL)
        return NULL;
    memset(d, 0, sizeof (*d));

    d->acc = mpeg2_bufalloc(ACC_SIZE, MPEG2_ALLOC_CHUNK);
    if (d->acc == NULL)
        return NULL;

    d->info.current_picture = &d->cur_pic;
    d->info.display_picture = NULL;
    d->info.display_fbuf = NULL;
    d->info.sequence = NULL;

    if (!decoder_start(d))
        return NULL;

    d->skip = 1;
    return d;
}

void mpeg2_close(mpeg2dec_t *d)
{
    if (d && d->st)
    {
        h264bsdShutdown(d->st);
        h264bsdFree(d->st);
        d->st = NULL;
    }
}

static void drain_output(mpeg2dec_t *d)
{
    u32 id, idr, nerr;
    while (h264bsdNextOutputPicture(d->st, &id, &idr, &nerr) != NULL)
        ;
}

void mpeg2_reset(mpeg2dec_t *d, int full)
{
    d->acc_len = d->acc_pos = 0;
    d->num_tags = 0;
    d->have_nal = 0;
    d->announced = 0;
    d->out_ready = 0;
    d->flushed = 0;
    d->pic_skip = 1;

    if (full)
    {
        mpeg2_close(d);
        mpeg2_mem_reset();
        d->have_seq = 0;
        d->info.sequence = NULL;
        d->info.display_picture = NULL;
        d->info.display_fbuf = NULL;
        decoder_start(d);
    }
    else if (d->st)
    {
        h264bsdFlushBuffer(d->st);
        drain_output(d);
    }
}

void mpeg2_skip(mpeg2dec_t *d, int skip)
{
    d->skip = skip;
}

void mpeg2_tag_picture(mpeg2dec_t *d, uint32_t tag1, uint32_t tag2)
{
    (void)tag2;
    if (d->num_tags == MAX_TAGS) /* drop the oldest */
    {
        memmove(d->tags, d->tags + 1, (MAX_TAGS - 1) * sizeof (d->tags[0]));
        d->num_tags--;
    }
    d->tags[d->num_tags].pos = d->acc_len;
    d->tags[d->num_tags].tag = tag1;
    d->num_tags++;
}

void mpeg2_buffer(mpeg2dec_t *d, const uint8_t *start, const uint8_t *end)
{
    uint32_t n = (uint32_t)(end - start);

    if (d->acc_len + n > ACC_SIZE)
        compact(d);

    if (d->acc_len + n > ACC_SIZE)
    {
        /* NAL too large to reassemble: drop what we hold and resync on the
         * next start code. */
        d->acc_len = d->acc_pos = 0;
        d->num_tags = 0;
        d->have_nal = 0;
        d->announced = 0;
        d->pic_skip = 1;
        if (n > ACC_SIZE)
            return;
    }

    memcpy(d->acc + d->acc_len, start, n);
    d->acc_len += n;
    d->flushed = 0;
}

int mpeg2_end(mpeg2dec_t *d)
{
    /* An access unit delimiter start code closes the last NAL */
    static const uint8_t eos[] = { 0, 0, 1, 0x09, 0x10 };

    if (d->flushed)
        return 0;
    mpeg2_buffer(d, eos, eos + sizeof eos);
    d->flushed = 1;
    return 1;
}

const mpeg2_info_t * mpeg2_info(mpeg2dec_t *d)
{
    return &d->info;
}

/* Feed the current NAL to libh264bsd. Returns 1 if a picture completed. */
static int decode_nal(mpeg2dec_t *d)
{
    uint8_t *p = d->acc + d->nal_off;
    u32 left = d->nal_len;
    int picready = 0, guard = 0;

    while (left > 0 && guard++ < 8)
    {
        u32 used = 0;
        u32 r = h264bsdDecode(d->st, p, left, d->cur_tag, &used);

        if (r == H264BSD_PIC_RDY)
            picready = 1;
        if (used == 0 && r != H264BSD_HDRS_RDY)
            break;
        p += used;
        left -= used;
        if (r == H264BSD_MEMALLOC_ERROR)
            break;
    }
    return picready;
}

mpeg2_state_t mpeg2_parse(mpeg2dec_t *d)
{
    if (d->st == NULL)
        return STATE_BUFFER;

    while (1)
    {
        if (d->out_ready)
        {
            u32 id, idr, nerr;
            uint8_t *pic = h264bsdNextOutputPicture(d->st, &id, &idr, &nerr);

            if (pic == NULL)
            {
                d->out_ready = 0;
                continue;
            }

            {
                const seqParamSet_t *sps = d->st->activeSps;
                if (sps && update_sequence(d, sps, 0))
                    return STATE_SEQUENCE; /* out_ready stays set */
            }

            {
                uint32_t left, top;
                uint32_t w = d->seq.width, h = d->seq.height;
                uint8_t *u = pic + w * h, *v = u + (w / 2) * (h / 2);

                crop_offsets(d, &left, &top);
                d->fbuf.buf[0] = pic + top * w + left;
                d->fbuf.buf[1] = u + (top / 2) * (w / 2) + left / 2;
                d->fbuf.buf[2] = v + (top / 2) * (w / 2) + left / 2;
            }

            d->disp_pic.tag = id;
            d->disp_pic.flags = (idr ? PIC_FLAG_CODING_TYPE_I :
                                       PIC_FLAG_CODING_TYPE_P) |
                                (id != H264DEC_NO_TAG ? PIC_FLAG_TAGS : 0);
            d->info.display_picture = &d->disp_pic;
            d->info.display_fbuf = &d->fbuf;
            return STATE_SLICE;
        }

        if (!d->have_nal)
        {
            if (!extract_nal(d))
                return STATE_BUFFER;
            d->have_nal = 1;
            d->announced = 0;
        }

        if ((d->nal_type == NAL_SLICE || d->nal_type == NAL_IDR) &&
            d->nal_first_slice && !d->announced)
        {
            d->cur_tag = take_tag(d);
            d->cur_flags = slice_is_idr(d) ? PIC_FLAG_CODING_TYPE_I :
                                             PIC_FLAG_CODING_TYPE_P;
            d->cur_pic.tag = d->cur_tag;
            d->cur_pic.flags = d->cur_flags |
                (d->cur_tag != H264DEC_NO_TAG ? PIC_FLAG_TAGS : 0);
            d->announced = 1;
            return STATE_PICTURE;
        }

        if (d->nal_type == NAL_SLICE || d->nal_type == NAL_IDR)
        {
            if (d->nal_first_slice)
                d->pic_skip = d->skip;
            if (d->pic_skip)
            {
                int report = d->nal_first_slice;
                consume_nal(d);
                if (!report)
                    continue;

                /* Like libmpeg2, still report a skipped picture (with its
                 * tag) so timestamps can be followed without decoding. The
                 * frame buffer, if any, is whatever was last displayed. */
                d->disp_pic.tag = d->cur_tag;
                d->disp_pic.flags = d->cur_flags | PIC_FLAG_SKIP |
                    (d->cur_tag != H264DEC_NO_TAG ? PIC_FLAG_TAGS : 0);
                d->info.display_picture = &d->disp_pic;
                return STATE_SLICE;
            }
        }

        if (decode_nal(d))
            d->out_ready = 1;

        if (d->nal_type == NAL_SPS)
        {
            uint32_t bp = 24, id;
            if (read_ue(d->acc + d->nal_hdr + 1, d->nal_len - 4, &bp, &id)
                && id < MAX_NUM_SEQ_PARAM_SETS && d->st->sps[id] != NULL
                && update_sequence(d, d->st->sps[id], 0))
            {
                consume_nal(d);
                return STATE_SEQUENCE;
            }
        }

        consume_nal(d);
    }
}
