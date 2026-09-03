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
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/unistd.h>

#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

#include "system.h"
#include "kernel.h"
#include "thread-psp.h"
#include "system-psp.h"
#include "panic.h"
#include "debug.h"

PSP_MODULE_INFO("rockbox", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

const char      *audiodev = NULL;
#ifdef DEBUG
bool debug_audio = false;
#endif

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    psp_sys_quit();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread,
                                     0x11, 0xFA0, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
}

void psp_sys_quit(void)
{
    sys_poweroff();
}

void power_off(void)
{
    struct thread_entry* t = sim_thread_unlock();
    sim_thread_shutdown();

    sim_thread_lock(t);
    while (1) yield();
}

void sim_do_exit(void)
{
    sim_kernel_shutdown();
    exit(EXIT_SUCCESS);
}

uintptr_t *stackbegin;
uintptr_t *stackend;

void system_init(void)
{
    volatile uintptr_t stack = 0;
    stackbegin = stackend = (uintptr_t*) &stack;

    setup_callbacks();
}

void system_reboot(void)
{
    sim_thread_exception_wait();
}

void system_exception_wait(void)
{
    system_reboot();
}

int hostfs_init(void)
{
    return 0;
}

/* PSPSDK's newlib declares ftruncate() (sys/unistd.h) but doesn't
 * implement it, and sceIo has no truncate primitive at all. Callers
 * (fileop.c, tagcache.c, plugin.c) use it opportunistically and can
 * tolerate failure. */
int ftruncate(int fd, off_t length)
{
    (void)fd; (void)length;
    errno = ENOSYS;
    return -1;
}

#ifdef HAVE_STORAGE_FLUSH
int hostfs_flush(void)
{
    return 0;
}
#endif /* HAVE_STORAGE_FLUSH */
