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

#include <pspaudiolib.h>

#include "config.h"
#include "sound.h"

/* PSP has no real hardware audio codec chip -- pspaudiolib talks straight
 * to sceAudio -- so the only gain stage Rockbox could drive here is
 * sceAudioChangeChannelVolume() (wrapped as pspAudioSetVolume()), a
 * per-channel digital attenuation applied to our own output before it ever
 * reaches the output amplifier.
 *
 * The PSP's physical volume keys are handled by the system below that
 * point and are the user's normal, muscle-memory volume control, so the
 * system volume is the single source of truth. Attenuating in the channel
 * gain as well would stack a second, independent stage on top of it: it
 * throws away bits of the signal before the system stage rather than
 * making anything louder, and leaves the same perceived loudness
 * reachable from two different controls. Pin our stage to unity and let
 * the OS own volume instead -- same reasoning as RG35XX_PRO in
 * drivers/audio/sdl.c, which defers to KNULLI's volume daemon.
 *
 * The Volume setting still exists in Settings (it comes from
 * sdl_codec.h's AUDIOHW_SETTING list) but is deliberately inert; the WPS
 * volume key bindings were dropped from keymap-psp.c to match. */
void audiohw_set_volume(int vol_l, int vol_r)
{
    (void)vol_l;
    (void)vol_r;

    /* Channel 0 is the only channel pcm-psp.c ever reserves. */
    pspAudioSetVolume(0, PSP_VOLUME_MAX, PSP_VOLUME_MAX);
}
