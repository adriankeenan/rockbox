/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Thin wrapper over libfaad for the player: ADTS framing, a private heap
 * (the plugin cannot use the codec API's allocator) and fixed-point output.
 * Compiled with the codec flags like libfaad itself.
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
#include "libfaad/common.h"
#include "libfaad/structs.h"
#include "libfaad/decoder.h"
#include "aac_dec.h"

void faad_heap_init(void *base, size_t size);
size_t faad_heap_used(void);

static NeAACDecHandle decoder;

static const unsigned adts_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
    16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

int adts_parse(const uint8_t *p, size_t len, unsigned *frame_len,
               unsigned *samplerate, unsigned *channels, unsigned *samples)
{
    unsigned flen, rate, ch, blocks;

    if (len < 7 || p[0] != 0xff || (p[1] & 0xf6) != 0xf0) /* sync, layer 0 */
        return -1;

    rate = adts_rates[(p[2] >> 2) & 0x0f];
    ch = ((p[2] & 1) << 2) | (p[3] >> 6);
    flen = ((p[3] & 3) << 11) | (p[4] << 3) | (p[5] >> 5);
    blocks = (p[6] & 3) + 1;

    if (rate == 0 || flen < 7)
        return -1;
    if (ch == 7)
        ch = 8;
    /* channel_config 0 means "in the stream" - the decoder finds out */

    if (frame_len)  *frame_len = flen;
    if (samplerate) *samplerate = rate;
    if (channels)   *channels = ch;
    if (samples)    *samples = 1024 * blocks;
    return 0;
}

bool aac_open(void *heap, size_t heap_size)
{
    NeAACDecConfigurationPtr conf;

    faad_heap_init(heap, heap_size);
    decoder = NeAACDecOpen();
    if (decoder == NULL)
        return false;

    conf = NeAACDecGetCurrentConfiguration(decoder);
    conf->outputFormat = FAAD_FMT_24BIT; /* not used: we take time_out */
    NeAACDecSetConfiguration(decoder, conf);
    return true;
}

void aac_close(void)
{
    decoder = NULL;   /* the heap is reset on the next open */
}

bool aac_init(const uint8_t *frame, size_t len)
{
    uint32_t rate = 0;
    unsigned char ch = 0;

    return decoder != NULL &&
        NeAACDecInit(decoder, (unsigned char *)frame, len, &rate, &ch) >= 0;
}

int aac_decode(const uint8_t *buf, size_t len, struct aac_info *info)
{
    NeAACDecFrameInfo fi;
    void *ret;

    ret = NeAACDecDecode(decoder, &fi, (unsigned char *)buf, len);

    info->consumed = fi.bytesconsumed;
    info->channels = fi.channels;
    info->samplerate = fi.samplerate;
    info->samples = fi.channels ? fi.samples / fi.channels : 0;
    info->pcm[0] = decoder->time_out[0];
    info->pcm[1] = decoder->time_out[fi.channels > 1 ? 1 : 0];

    return (ret == NULL || fi.error > 0 || fi.channels == 0 ||
            fi.channels > 2) ? -1 : 0;
}

void aac_post_seek(void)
{
    if (decoder)
        NeAACDecPostSeekReset(decoder, -1);
}

size_t aac_heap_used(void)
{
    return faad_heap_used();
}
