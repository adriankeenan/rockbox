/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockbox PSP port
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <pspkernel.h>
#include <pspaudiolib.h>
#include <pspthreadman.h>

#include "config.h"
#include "pcm.h"
#include "pcm-internal.h"
#include "pcm_sampr.h"
#include "pcm_mixer.h"
#include "pcm_sink.h"

/* pcm_mtx has to be reentrant: the generic core's pcm_play_data() (called
 * from the codec thread to start playback) calls pcm_play_lock() --
 * sinks[cur_sink]->ops.lock(), i.e. sink_lock() below -- and, while still
 * holding it, calls sinks[cur_sink]->ops.play() (sink_dma_start()), which
 * itself calls sink_lock() again from the same thread. A plain
 * sceKernelCreateSema() semaphore isn't reentrant, so this deadlocked the
 * calling thread solid the moment playback ever actually started -- same
 * class of bug as thread-psp.c's reclock and kernel-psp.c's irq_mtx. */
typedef struct
{
    SceUID sema;
    SceUID owner;
    int count;
} psp_pcmlock_t;

static psp_pcmlock_t pcm_mtx;

/* Bytes left in the Rockbox PCM frame buffer. */
static size_t _pcm_buffer_size = 0;
static const void *_pcm_buffer = NULL;
static volatile bool _channel_active = false;

static void sink_lock(void)
{
    SceUID self = sceKernelGetThreadId();
    if (pcm_mtx.owner == self)
    {
        pcm_mtx.count++;
        return;
    }
    sceKernelWaitSema(pcm_mtx.sema, 1, NULL);
    pcm_mtx.owner = self;
    pcm_mtx.count = 1;
}

static void sink_unlock(void)
{
    if (--pcm_mtx.count == 0)
    {
        pcm_mtx.owner = -1;
        sceKernelSignalSema(pcm_mtx.sema, 1);
    }
}

static void psp_audio_callback(void *buf, unsigned int reqn, void *pdata)
{
    (void)pdata;
    uint8_t *out = (uint8_t *)buf;
    size_t bytes_needed = (size_t)reqn * 4; /* stereo, 16-bit */

    sink_lock();

    while (bytes_needed > 0)
    {
        if (_pcm_buffer_size == 0)
        {
            if (!pcm_play_dma_complete_callback(PCM_DMAST_OK, &_pcm_buffer,
                                                &_pcm_buffer_size))
            {
                memset(out, 0, bytes_needed);
                sink_unlock();
                return;
            }
            pcm_play_dma_status_callback(PCM_DMAST_STARTED);
        }

        size_t copy = bytes_needed < _pcm_buffer_size ?
                       bytes_needed : _pcm_buffer_size;
        memcpy(out, _pcm_buffer, copy);
        out += copy;
        _pcm_buffer = (const uint8_t *)_pcm_buffer + copy;
        _pcm_buffer_size -= copy;
        bytes_needed -= copy;
    }

    sink_unlock();
}

static void sink_dma_init(void)
{
    pcm_mtx.sema = sceKernelCreateSema("rb_pcm_mtx", 0, 1, 1, NULL);
    pcm_mtx.owner = -1;
    pcm_mtx.count = 0;
    pspAudioInit();
}

static void sink_dma_postinit(void)
{
}

static void sink_set_freq(uint16_t freq)
{
    (void)freq; /* pspaudiolib channel 0 runs at a fixed 44.1kHz */
}

static void sink_dma_start(const void *addr, size_t size)
{
    sink_lock();
    _pcm_buffer = addr;
    _pcm_buffer_size = size;
    sink_unlock();

    if (!_channel_active)
    {
        pspAudioSetChannelCallback(0, psp_audio_callback, NULL);
        _channel_active = true;
    }
}

static void sink_dma_stop(void)
{
    if (_channel_active)
    {
        pspAudioSetChannelCallback(0, NULL, NULL);
        _channel_active = false;
    }
}

void pcm_close_device(void)
{
    sink_dma_stop();
    pspAudioEnd();
}

void audiohw_close(void)
{
    pcm_close_device();
}

struct pcm_sink builtin_pcm_sink = {
    .caps = {
        .samprs       = hw_freq_sampr,
        .num_samprs   = HW_NUM_FREQ,
        .default_freq = HW_FREQ_DEFAULT,
    },
    .ops = {
        .init     = sink_dma_init,
        .postinit = sink_dma_postinit,
        .set_freq = sink_set_freq,
        .lock     = sink_lock,
        .unlock   = sink_unlock,
        .play     = sink_dma_start,
        .stop     = sink_dma_stop,
    },
};
