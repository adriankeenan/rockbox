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

/* PSPSDK has no dlopen()/dlfcn.h (there is no real dynamic ELF loader on
 * this SDK), so dynamically-loadable codecs and plugins can't be supported
 * this way yet. This stub keeps the core linking against CONFIG_BINFMT ==
 * BINFMT_DLOPEN; lc_open() always fails cleanly rather than crashing.
 * TODO: either write a custom relocating loader (as the CTRU port does via
 * CTRDL) or switch codecs/plugins to be statically linked instead. */

#include <stddef.h>

#include "debug.h"

void *lc_open(const char *filename, unsigned char *buf, size_t buf_size)
{
    (void)buf; (void)buf_size;
    DEBUGF("lc_open(%s): dynamic loading not supported on PSP\n", filename);
    return NULL;
}

void *lc_get_header(void *handle)
{
    (void)handle;
    return NULL;
}

void lc_close(void *handle)
{
    (void)handle;
}
