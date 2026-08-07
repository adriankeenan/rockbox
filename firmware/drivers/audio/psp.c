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

#include "config.h"
#include "sound.h"

/* PSP has no real hardware audio codec chip (pspaudiolib talks straight to
 * sceAudio). Like the CTRU port (which is in the same boat with NDSP),
 * volume is a no-op "hardware" control for now.
 * TODO: apply gain in the psp_audio_callback() PCM mixer in pcm-psp.c
 * instead (pspAudioSetVolume() takes a raw linear 0-0x8000 gain, so this
 * needs its own conversion from Rockbox's centibel scale rather than being
 * a real per-target audiohw register write). */
void audiohw_set_volume(int vol_l, int vol_r)
{
    (void)vol_l; (void)vol_r;
}
