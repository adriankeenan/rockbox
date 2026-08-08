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

#include <math.h>
#include <pspaudiolib.h>

#include "config.h"
#include "sound.h"

/* PSP has no real hardware audio codec chip (pspaudiolib talks straight to
 * sceAudio), but sceAudioChangeChannelVolume() (wrapped here as
 * pspAudioSetVolume()) gives the audio driver/hardware a real per-channel
 * gain control -- so, unlike CTRU (which has no such API for NDSP and
 * stays a true no-op), volume here can be delegated to the platform
 * instead of scaling PCM samples ourselves in psp_audio_callback().
 *
 * Rockbox passes vol_l/vol_r in centibels (tenths of a dB); sdl_codec.h's
 * AUDIOHW_SETTING(VOLUME, ...) gives us a -80..0dB range. pspAudioSetVolume
 * wants a raw linear 0..PSP_VOLUME_MAX gain, so convert via the standard
 * dB-to-amplitude relation: gain = PSP_VOLUME_MAX * 10^(dB/20), and since
 * vol_cb is in tenths of a dB, dB/20 == vol_cb/200. */
static int cb_to_psp_gain(int vol_cb)
{
    if (vol_cb <= -800)
        return 0;
    if (vol_cb >= 0)
        return PSP_VOLUME_MAX;

    return (int)(PSP_VOLUME_MAX * powf(10.0f, vol_cb / 200.0f));
}

void audiohw_set_volume(int vol_l, int vol_r)
{
    /* Channel 0 is the only channel pcm-psp.c ever reserves. */
    pspAudioSetVolume(0, cb_to_psp_gain(vol_l), cb_to_psp_gain(vol_r));
}
