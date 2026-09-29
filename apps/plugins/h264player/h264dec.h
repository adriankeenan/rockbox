/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * H.264 decoder front end for the player. It keeps the small subset of the
 * libmpeg2 API that the video thread was written against (states, info
 * structs, mpeg2_parse/buffer/skip/reset/tag_picture) but decodes H.264
 * baseline through libh264bsd.
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
#ifndef H264DEC_H
#define H264DEC_H

#include <stdint.h>
#include <stddef.h>

#define MPEG2_COMPONENTS 3
#define H264DEC_NO_TAG   0xffffffffu

/* Sequence flags are unused by the player but kept for source compatibility */
#define PIC_MASK_CODING_TYPE   7
#define PIC_FLAG_CODING_TYPE_I 1  /* IDR picture */
#define PIC_FLAG_CODING_TYPE_P 2  /* anything else */
#define PIC_FLAG_CODING_TYPE_B 3  /* never produced (baseline) */
#define PIC_FLAG_CODING_TYPE_D 4
#define PIC_FLAG_SKIP          64
#define PIC_FLAG_TAGS          128

typedef struct mpeg2_sequence_s
{
    unsigned int width, height;              /* luma stride, MB aligned height */
    unsigned int chroma_width, chroma_height;
    unsigned int display_width, display_height; /* cropped size */
    unsigned int frame_period;               /* 27MHz clock ticks */
} mpeg2_sequence_t;

typedef struct mpeg2_picture_s
{
    uint32_t tag;
    uint32_t flags;
} mpeg2_picture_t;

typedef struct mpeg2_fbuf_s
{
    uint8_t * buf[MPEG2_COMPONENTS];
} mpeg2_fbuf_t;

typedef struct mpeg2_info_s
{
    const mpeg2_sequence_t * sequence;
    const mpeg2_picture_t * current_picture;
    const mpeg2_fbuf_t * display_fbuf;
    const mpeg2_picture_t * display_picture;
} mpeg2_info_t;

typedef enum
{
    STATE_BUFFER,       /* needs more data */
    STATE_SEQUENCE,     /* new/changed sequence parameters */
    STATE_PICTURE,      /* a picture is about to be decoded */
    STATE_SLICE,        /* a picture was decoded, display_* is valid */
    STATE_END,
    STATE_INVALID_END,
} mpeg2_state_t;

typedef struct mpeg2dec_s mpeg2dec_t;

mpeg2dec_t * mpeg2_init(void);
void mpeg2_close(mpeg2dec_t * dec);
/* full: also forget the sequence and release all decoder memory */
void mpeg2_reset(mpeg2dec_t * dec, int full);
void mpeg2_skip(mpeg2dec_t * dec, int skip);
void mpeg2_buffer(mpeg2dec_t * dec, const uint8_t * start, const uint8_t * end);
/* No more data will follow: terminate the final NAL. Returns non-zero if
 * something new can now be parsed (call mpeg2_parse again), 0 if already
 * flushed. Re-armed by mpeg2_buffer/mpeg2_reset. */
int mpeg2_end(mpeg2dec_t * dec);
/* Tag the next picture starting in data given to mpeg2_buffer */
void mpeg2_tag_picture(mpeg2dec_t * dec, uint32_t tag1, uint32_t tag2);
mpeg2_state_t mpeg2_parse(mpeg2dec_t * dec);
const mpeg2_info_t * mpeg2_info(mpeg2dec_t * dec);

/* Allocator supplied by the host application */
typedef enum
{
    MPEG2_ALLOC_MPEG2DEC   = 0,
    MPEG2_ALLOC_CHUNK      = 1,
    MPEG2_ALLOC_YUV        = 2,
    MPEG2_ALLOC_CONVERT_ID = 3,
    MPEG2_ALLOC_CONVERTED  = 4,
    MPEG_ALLOC_CODEC_MALLOC,
    MPEG_ALLOC_CODEC_CALLOC,
    MPEG_ALLOC_MPEG2_BUFFER,
    MPEG_ALLOC_AUDIOBUF,
    MPEG_ALLOC_PCMOUT,
    MPEG_ALLOC_DISKBUF,
    __MPEG_ALLOC_FIRST = -256,
} mpeg2_alloc_t;

/* Zero-initialised, 32-bit aligned; released by mpeg2_mem_reset */
void * mpeg2_malloc(unsigned size, mpeg2_alloc_t reason);
/* allocates a dedicated buffer and locks all previous allocation in place */
void * mpeg2_bufalloc(unsigned size, mpeg2_alloc_t reason);
/* clears all non-dedicated buffer space */
void mpeg2_mem_reset(void);
void mpeg2_alloc_init(unsigned char* buf, int mallocsize);

#endif /* H264DEC_H */
