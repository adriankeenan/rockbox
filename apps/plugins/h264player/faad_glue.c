/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * What libfaad and the MDCT/FFT library expect from the codec API
 * (codeclib.c), provided for a plugin: a private bump allocator and the
 * bit-scan lookup tables. Nothing here touches the codec API itself.
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "platform.h"
#include "codecs.h"
#include "codeclib.h"

/* Referenced only as an extern by codeclib.h; the code we link never uses it */
struct codec_api *ci;

static unsigned char *heap_base;
static size_t heap_size, heap_pos;

void faad_heap_init(void *base, size_t size)
{
    heap_base = base;
    heap_size = size;
    heap_pos = 0;
}

size_t faad_heap_used(void)
{
    return heap_pos;
}

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef memset
#undef memcpy

void* codec_malloc(size_t size)
{
    void *x;

    size = (size + 7) & ~(size_t)7;
    if (heap_base == NULL || heap_pos + size > heap_size)
        return NULL;

    x = heap_base + heap_pos;
    heap_pos += size;
    return x;
}

void* codec_calloc(size_t nmemb, size_t size)
{
    void *x = codec_malloc(nmemb * size);

    if (x != NULL)
        memset(x, 0, nmemb * size);
    return x;
}

void codec_free(void *ptr)
{
    (void)ptr;
}

void* codec_realloc(void *ptr, size_t size)
{
    void *x = codec_malloc(size);

    if (x != NULL && ptr != NULL)
        memcpy(x, ptr, size);   /* may read past the old block; harmless here */
    return x;
}

size_t codec_strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;
    return p - s;
}

const uint8_t bs_log2_tab[256] ICONST_ATTR = {
    0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const uint8_t bs_clz_tab[256] ICONST_ATTR = {
    8,7,6,6,5,5,5,5,4,4,4,4,4,4,4,4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* libfaad sorts a handful of band-table entries (sbr_fbt.c); the firmware libc
 * has no qsort, so provide a small insertion sort. Renamed via -Dqsort so it
 * cannot clash with a libc that does have one. */
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    unsigned char *b = base;
    size_t i, j, k;

    for (i = 1; i < nmemb; i++)
    {
        for (j = i; j > 0 && compar(b + (j - 1) * size, b + j * size) > 0; j--)
        {
            unsigned char *x = b + (j - 1) * size, *y = b + j * size;

            for (k = 0; k < size; k++)
            {
                unsigned char t = x[k];
                x[k] = y[k];
                y[k] = t;
            }
        }
    }
}
