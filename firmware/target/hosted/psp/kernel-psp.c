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

#include <pspkernel.h>
#include <pspthreadman.h>

#include "system-psp.h"
#include "thread-psp.h"
#include "kernel.h"
#include "thread.h"
#include "panic.h"
#include "debug.h"

long start_tick;

/* Mutex serializing level changes and excluding threads while a "handler"
 * runs, plus a semaphore acting as a broadcast condition for when
 * interrupts become re-enabled. */
static SceUID irq_mtx;
static SceUID irq_cond_sema;
static SceUID tick_thread_id = -1;
static volatile bool tick_thread_should_run;

/* Level: 0 = enabled, not 0 = disabled */
static int volatile interrupt_level = HIGHEST_IRQ_LEVEL;
static int handlers_pending = 0;
static int status_reg = 0;

int set_irq_level(int level)
{
    sceKernelWaitSema(irq_mtx, 1, NULL);

    int oldlevel = interrupt_level;

    if (status_reg == 0 && level == 0 && oldlevel != 0)
    {
        if (handlers_pending > 0)
            sceKernelSignalSema(irq_cond_sema, handlers_pending);
    }

    interrupt_level = level;

    sceKernelSignalSema(irq_mtx, 1);
    return oldlevel;
}

void sim_enter_irq_handler(void)
{
    sceKernelWaitSema(irq_mtx, 1, NULL);
    handlers_pending++;

    while (interrupt_level != 0)
    {
        sceKernelSignalSema(irq_mtx, 1);
        sceKernelWaitSema(irq_cond_sema, 1, NULL);
        sceKernelWaitSema(irq_mtx, 1, NULL);
    }

    status_reg = 1;
}

void sim_exit_irq_handler(void)
{
    if (--handlers_pending > 0)
        sceKernelSignalSema(irq_cond_sema, 1);

    status_reg = 0;
    sceKernelSignalSema(irq_mtx, 1);
}

static int tick_thread_func(SceSize args, void *argp)
{
    (void)args; (void)argp;

    while (tick_thread_should_run)
    {
        sceKernelDelayThread(1000000 / HZ);

        sim_enter_irq_handler();
        call_tick_tasks();
        sim_exit_irq_handler();
    }

    return 0;
}

void sim_kernel_shutdown(void)
{
    tick_thread_should_run = false;
    enable_irq();

    if (tick_thread_id >= 0)
    {
        sceKernelWaitThreadEnd(tick_thread_id, NULL);
        sceKernelDeleteThread(tick_thread_id);
        tick_thread_id = -1;
    }

    while (handlers_pending > 0)
        sceKernelDelayThread(10000);
}

void tick_start(unsigned int interval_in_ms)
{
    (void)interval_in_ms;

    irq_mtx = sceKernelCreateSema("rb_irq_mtx", 0, 1, 1, NULL);
    irq_cond_sema = sceKernelCreateSema("rb_irq_cond", 0, 0, 255, NULL);

    start_tick = sceKernelGetSystemTimeLow() / 1000;

    tick_thread_should_run = true;
    tick_thread_id = sceKernelCreateThread("rb_tick", tick_thread_func,
                                           0x12, 16*1024, 0, NULL);
    if (tick_thread_id >= 0)
        sceKernelStartThread(tick_thread_id, 0, NULL);
}
