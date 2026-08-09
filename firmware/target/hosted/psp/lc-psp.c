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

/* PSPSDK has no dlopen()/dlfcn.h, and psp-gcc's eabi ABI can't generate PIC
 * code at all, so a real ELF dynamic loader (like CTRU's CTRDL) isn't an
 * option here. Instead, codecs are linked at a fixed address -- the same
 * approach every PLATFORM_NATIVE target uses (CONFIG_BINFMT == BINFMT_ROCK,
 * see the PSP branch in apps/plugins/plugin.lds) -- so loading one is just
 * a raw read() into memory rather than any real relocation/linking work.
 * This mirrors firmware/lc-rock.c almost exactly. */

#include "config.h"
#include "system.h"
#include "kernel.h"
#include "file.h"
#include "debug.h"
#include "load_code.h"

void *lc_open(const char *filename, unsigned char *buf, size_t buf_size)
{
    int fd = open(filename, O_RDONLY);
    ssize_t read_size;
    struct lc_header hdr;
    unsigned char *buf_end = buf + buf_size;
    off_t copy_size;

    if (fd < 0)
    {
        DEBUGF("lc_open: could not open %s\n", filename);
        goto error;
    }

    /* read the header to obtain the load address */
    read_size = read(fd, &hdr, sizeof(hdr));

    if (read_size < 0)
    {
        DEBUGF("lc_open: could not read header from %s\n", filename);
        goto error_fd;
    }

    /* hdr.end_addr points to the end of the bss section, but there might
     * be idata/icode behind that so the bytes to copy can be larger */
    copy_size = MAX(filesize(fd), hdr.end_addr - hdr.load_addr);

    if (hdr.load_addr < buf || (hdr.load_addr + copy_size) > buf_end)
    {
        DEBUGF("lc_open: %s doesn't fit into the codec buffer\n", filename);
        goto error_fd;
    }

    /* go back to the beginning to load the whole thing (incl. header) */
    if (lseek(fd, 0, SEEK_SET) < 0)
    {
        DEBUGF("lc_open: lseek failed on %s\n", filename);
        goto error_fd;
    }

    /* the header has the address where the code is linked at */
    read_size = read(fd, hdr.load_addr, copy_size);
    close(fd);

    if (read_size < 0)
    {
        DEBUGF("lc_open: could not read %s\n", filename);
        goto error;
    }

    /* the file (built via objcopy -O binary from a NOLOAD .bss section)
     * only contains .header/.text/.rodata/.data -- codec_start() zeroes
     * the BSS region itself before the codec's real entry point runs. */
    commit_discard_idcache();
    return hdr.load_addr;

error_fd:
    close(fd);
error:
    return NULL;
}
