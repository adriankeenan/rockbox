/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * MPEG transport stream (TS) demuxer for the player
 *
 * Each 188-byte TS packet's payload is contiguous in the disk buffer, so it is
 * handed out directly as a "packet" of the stream, the same way the program
 * stream parser hands out PES payloads. A payload chunk that begins a PES
 * packet carries that packet's PTS.
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
#include "plugin.h"
#include "h264player.h"
#include "ts_parser.h"

#define TS_PKT       188
#define TS_SYNC      0x47
#define TS_NO_PID    0x1fff   /* null packets: never matches a real stream */
#define PSI_PROBE    (128*1024)

/* stream_type values from the PMT */
#define ST_MPEG1_AUDIO 0x03
#define ST_MPEG2_AUDIO 0x04
#define ST_AAC_ADTS    0x0f
#define ST_H264        0x1b

static struct
{
    off_t    base;        /* file offset of the first packet mod TS_PKT */
    unsigned video_pid;
    unsigned audio_pid;
} ts = { 0, TS_NO_PID, TS_NO_PID };

int audio_codec = AUDIO_CODEC_MPA;

static inline unsigned pkt_pid(const uint8_t *p)
{
    return ((p[1] & 0x1f) << 8) | p[2];
}

/* Offset of the payload within a packet, or -1 if there is none/it is
 * malformed */
static inline int pkt_payload_off(const uint8_t *p)
{
    int afc = (p[3] >> 4) & 3;
    int off = 4;

    if (!(afc & 1))
        return -1;              /* adaptation field only */
    if (afc & 2)
        off += 1 + p[4];        /* adaptation_field_length + itself */
    return off < TS_PKT ? off : -1;
}

/* If this packet starts a PES packet, return the offset of the elementary
 * stream data and fill *pts (INVALID_TIMESTAMP if none). Returns -1 if it does
 * not start a PES packet. */
static int pes_start(const uint8_t *p, int off, uint32_t *pts)
{
    const uint8_t *h = p + off;
    int hlen;

    *pts = INVALID_TIMESTAMP;

    if (!(p[1] & 0x40) || TS_PKT - off < 9)
        return -1;
    if (h[0] != 0 || h[1] != 0 || h[2] != 1)
        return -1;

    hlen = 9 + h[8];

    if ((h[7] & 0x80) && TS_PKT - off >= 14 && TS_CHECK_MARKERS(h, 9))
        *pts = TS_FROM_HEADER(h, 9);

    return hlen < TS_PKT - off ? off + hlen : TS_PKT;
}

static inline off_t align_down(off_t pos)
{
    off_t r = (pos - ts.base) % TS_PKT;
    if (r < 0)
        r += TS_PKT;
    return pos - r;
}

/** PSI **/

static bool parse_pat(const uint8_t *p, int off, unsigned *pmt_pid)
{
    const uint8_t *s = p + off;
    int len, i;

    if (!(p[1] & 0x40))
        return false;
    s += 1 + s[0];                    /* pointer_field */
    if (s >= p + TS_PKT - 12 || s[0] != 0x00)
        return false;
    len = ((s[1] & 0x0f) << 8) | s[2];
    for (i = 8; i + 4 <= len - 1 && s + i + 4 <= p + TS_PKT; i += 4)
    {
        unsigned prog = (s[i] << 8) | s[i + 1];
        if (prog != 0)
        {
            *pmt_pid = ((s[i + 2] & 0x1f) << 8) | s[i + 3];
            return true;
        }
    }
    return false;
}

static void parse_pmt(const uint8_t *p, int off)
{
    const uint8_t *s = p + off;
    const uint8_t *end;
    int len, pil;

    if (!(p[1] & 0x40))
        return;
    s += 1 + s[0];
    if (s >= p + TS_PKT - 12 || s[0] != 0x02)
        return;
    len = ((s[1] & 0x0f) << 8) | s[2];
    end = s + 3 + len - 4;                     /* drop CRC */
    if (end > p + TS_PKT)
        end = p + TS_PKT;
    pil = ((s[10] & 0x0f) << 8) | s[11];
    s += 12 + pil;

    while (s + 5 <= end)
    {
        unsigned type = s[0];
        unsigned pid = ((s[1] & 0x1f) << 8) | s[2];
        int esil = ((s[3] & 0x0f) << 8) | s[4];

        if (type == ST_H264 && ts.video_pid == TS_NO_PID)
            ts.video_pid = pid;
        else if ((type == ST_MPEG1_AUDIO || type == ST_MPEG2_AUDIO) &&
                 ts.audio_pid == TS_NO_PID)
        {
            ts.audio_pid = pid;
            audio_codec = AUDIO_CODEC_MPA;
        }
        else if (type == ST_AAC_ADTS && ts.audio_pid == TS_NO_PID)
        {
            ts.audio_pid = pid;
            audio_codec = AUDIO_CODEC_AAC;
        }
        s += 5 + esil;
    }
}

bool ts_probe(void)
{
    uint8_t *p;
    ssize_t len;
    int i, k;
    unsigned pmt_pid = TS_NO_PID;

    ts.video_pid = ts.audio_pid = TS_NO_PID;
    audio_codec = AUDIO_CODEC_MPA;

    if (disk_buf_lseek(0, SEEK_SET) < 0)
        return false;
    len = disk_buf_getbuffer(PSI_PROBE, &p, NULL, NULL);
    if (len < 4 * TS_PKT)
        return false;

    /* Find packet alignment: sync bytes must repeat every 188 bytes */
    for (i = 0; i < TS_PKT; i++)
    {
        for (k = 0; k < 4; k++)
            if (p[i + k * TS_PKT] != TS_SYNC)
                break;
        if (k == 4)
            break;
    }
    if (i == TS_PKT)
        return false;

    ts.base = i;

    for (; i + TS_PKT <= len; i += TS_PKT)
    {
        const uint8_t *pk = p + i;
        int off;
        unsigned pid;

        if (pk[0] != TS_SYNC)
            break;
        pid = pkt_pid(pk);
        off = pkt_payload_off(pk);
        if (off < 0)
            continue;

        if (pid == 0 && pmt_pid == TS_NO_PID)
            parse_pat(pk, off, &pmt_pid);
        else if (pid == pmt_pid)
        {
            parse_pmt(pk, off);
            break;
        }
    }

    DEBUGF("ts_probe: base:%ld video pid:%u audio pid:%u codec:%d\n",
           (long)ts.base, ts.video_pid, ts.audio_pid, audio_codec);

    return ts.video_pid != TS_NO_PID;
}

static inline unsigned stream_pid(unsigned id)
{
    return STREAM_IS_VIDEO(id) ? ts.video_pid : ts.audio_pid;
}

/** Streaming / random access **/

int ts_next_data(struct stream *str, enum stream_parse_mode type)
{
    #define INC_BUF(offset) \
        ({ off_t _o = (offset);           \
           str->hdr.win_right += _o;      \
           if ((p += _o) >= disk_buf.end) \
                p -= disk_buf.size; })

    const unsigned pid = stream_pid(str->id);
    uint8_t *p = str->curr_packet_end;

    str->pkt_flags = 0;

    while (1)
    {
        int off, es;
        uint32_t pts;

        switch (type)
        {
        case STREAM_PM_STREAMING:
            switch (str->state)
            {
            case SSTATE_PARSE:
                if (str->hdr.win_left < disk_buf.filesize)
                    break;
                str_end_of_stream(str);
                return STREAM_DATA_END;

            case SSTATE_SYNC:
                if (str->hdr.win_right < disk_buf.filesize)
                    break;
                str_end_of_stream(str);
                /* Fall-through */
            case SSTATE_END:
                return STREAM_DATA_END;
            }

            if (!disk_buf_is_data_ready(&str->hdr, MIN_BUFAHEAD))
            {
                int res = str_next_data_not_ready(str);

                if (res != STREAM_OK)
                    return res;
            }
            break;

        case STREAM_PM_RANDOM_ACCESS:
            str->hdr.pos = disk_buf_lseek(str->hdr.pos, SEEK_SET);

            if (str->hdr.pos < 0 || str->hdr.pos >= str->hdr.limit ||
                disk_buf_getbuffer(MIN_BUFAHEAD, &p, NULL, NULL) <= 0)
            {
                str_end_of_stream(str);
                return STREAM_DATA_END;
            }

            str->state = SSTATE_SYNC;
            str->hdr.win_left = str->hdr.pos;
            str->curr_packet = NULL;
            str->curr_packet_end = p;
            break;
        }

        /* A whole packet must remain in the file */
        if (str->hdr.win_right + TS_PKT > disk_buf.filesize)
        {
            str_end_of_stream(str);
            return STREAM_DATA_END;
        }

        if (str->state == SSTATE_SYNC)
        {
            /* Two consecutive sync bytes to be sure of the alignment. The
             * second lies within MIN_BUFAHEAD unless we are at the very end,
             * where the guard space still holds valid data or the check
             * simply fails and we skip a byte. */
            if (p[0] != TS_SYNC ||
                (str->hdr.win_right + 2*TS_PKT <= disk_buf.filesize &&
                 p[TS_PKT] != TS_SYNC))
            {
                INC_BUF(1);
                continue;
            }
        }

        str->state = SSTATE_PARSE;

        if (p[0] != TS_SYNC)
        {
            /* Lost sync - go looking for it again */
            str->state = SSTATE_SYNC;
            INC_BUF(1);
            continue;
        }

        off = pkt_payload_off(p);

        if (pkt_pid(p) != pid || off < 0)
        {
            INC_BUF(TS_PKT);
            continue;
        }

        es = pes_start(p, off, &pts);

        if (es >= 0)
        {
            if (pts != INVALID_TIMESTAMP)
            {
                str->pts = pts;
                str->pkt_flags |= PKT_HAS_TS;
            }
        }
        else
        {
            es = off;   /* continuation of a PES packet */
        }

        str->curr_packet = p + es;
        str->curr_packet_end = p + TS_PKT;
        str->hdr.win_left = str->hdr.win_right + es;
        str->hdr.win_right += TS_PKT;

        return STREAM_OK;
    }

    #undef INC_BUF
}

/** Scanning (seeking) **/

/* Read the packet at sk->pos; NULL if it cannot be read */
static const uint8_t * scan_packet(struct stream_scan *sk)
{
    uint8_t *p;
    off_t pos = disk_buf_lseek(sk->pos, SEEK_SET);

    if (pos < 0 || disk_buf_getbuffer(TS_PKT, &p, NULL, NULL) < TS_PKT)
        return NULL;

    return p[0] == TS_SYNC ? p : NULL;
}

static void scan_start(struct stream_scan *sk)
{
    stream_scan_normalize(sk);
    sk->pos = align_down(sk->pos);

    if (sk->dir < 0)
        stream_scan_offset(sk, TS_PKT);
    else if (sk->pos < 0)
        sk->pos = 0;
}

/* Common scanning loop. If need_sps, the packet must also begin an IDR access
 * unit, which is recognised by an SPS NAL in the packet's first payload. */
static uint32_t scan_pes(struct stream_scan *sk, unsigned pid, bool need_sps)
{
    scan_start(sk);

    while (sk->len >= 0 && sk->margin >= TS_PKT && sk->pos >= 0)
    {
        const uint8_t *p = scan_packet(sk);

        if (p == NULL)
            break;

        if (pkt_pid(p) == pid)
        {
            int off = pkt_payload_off(p);
            uint32_t pts;
            int es = off >= 0 ? pes_start(p, off, &pts) : -1;

            if (es >= 0 && pts != INVALID_TIMESTAMP)
            {
                bool ok = true;

                if (need_sps)
                {
                    int i;
                    ok = false;
                    for (i = es; i + 4 <= TS_PKT; i++)
                    {
                        if (p[i] == 0 && p[i+1] == 0 && p[i+2] == 1 &&
                            (p[i+3] & 0x9f) == 7)
                        {
                            ok = true;
                            break;
                        }
                    }
                }

                if (ok)
                {
                    sk->data = TS_PKT;
                    return pts;
                }
            }
        }

        stream_scan_offset(sk, TS_PKT);
    }

    return INVALID_TIMESTAMP;
}

uint32_t ts_scan_pts(struct stream_scan *sk, unsigned id)
{
    return scan_pes(sk, stream_pid(id), false);
}

uint32_t ts_scan_keyframe(struct stream_scan *sk)
{
    return scan_pes(sk, ts.video_pid, true);
}
