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

/* Origin for the tick clock, in microseconds. Deliberately not derived from
 * start_tick: sceKernelGetSystemTimeLow() is only the low 32 bits of the
 * microsecond clock and so wraps every ~72 minutes, which is survivable for
 * start_tick's use in sleep_thread() (a modulo, so a wrap costs one skewed
 * sleep) but not for a clock current_tick is computed from -- a wrap there
 * would send the catch-up loop below spinning. Wide is 64-bit. */
static SceInt64 tick_origin_us;

/* irq_mtx has to be reentrant: sim_enter_irq_handler() holds it for the
 * whole duration of call_tick_tasks() (i.e. across button_tick() and
 * friends), and tick tasks routinely call disable_irq()/restore_irq()
 * themselves (queue_post() does, for every posted button event) which
 * both go through set_irq_level() below and so need to re-acquire this
 * same lock from the same (tick) thread without deadlocking. A plain
 * sceKernelCreateSema() semaphore isn't reentrant, so track the owner
 * and a recursion count the same way thread-psp.c's reclock does. */
typedef struct
{
    SceUID sema;
    SceUID owner;
    int count;
} psp_irqlock_t;

static psp_irqlock_t irq_mtx;
static SceUID irq_cond_sema;
static SceUID tick_thread_id = -1;
static volatile bool tick_thread_should_run;

/* Level: 0 = enabled, not 0 = disabled */
static int volatile interrupt_level = HIGHEST_IRQ_LEVEL;
static int handlers_pending = 0;
static int status_reg = 0;

static void irqlock_init(psp_irqlock_t *l)
{
    l->sema = sceKernelCreateSema("rb_irq_mtx", 0, 1, 1, NULL);
    l->owner = -1;
    l->count = 0;
}

static void irqlock_lock(psp_irqlock_t *l)
{
    SceUID self = sceKernelGetThreadId();
    if (l->owner == self)
    {
        l->count++;
        return;
    }
    sceKernelWaitSema(l->sema, 1, NULL);
    l->owner = self;
    l->count = 1;
}

static void irqlock_unlock(psp_irqlock_t *l)
{
    if (--l->count == 0)
    {
        l->owner = -1;
        sceKernelSignalSema(l->sema, 1);
    }
}

int set_irq_level(int level)
{
    irqlock_lock(&irq_mtx);

    int oldlevel = interrupt_level;

    if (status_reg == 0 && level == 0 && oldlevel != 0)
    {
        if (handlers_pending > 0)
            sceKernelSignalSema(irq_cond_sema, handlers_pending);
    }

    interrupt_level = level;

    irqlock_unlock(&irq_mtx);
    return oldlevel;
}

void sim_enter_irq_handler(void)
{
    irqlock_lock(&irq_mtx);
    handlers_pending++;

    while (interrupt_level != 0)
    {
        irqlock_unlock(&irq_mtx);
        sceKernelWaitSema(irq_cond_sema, 1, NULL);
        irqlock_lock(&irq_mtx);
    }

    status_reg = 1;
}

void sim_exit_irq_handler(void)
{
    if (--handlers_pending > 0)
        sceKernelSignalSema(irq_cond_sema, 1);

    status_reg = 0;
    irqlock_unlock(&irq_mtx);
}

static int tick_thread_func(SceSize args, void *argp)
{
    (void)args; (void)argp;

    while (tick_thread_should_run)
    {
        long new_tick;

        sceKernelDelayThread(1000000 / HZ);

        /* Drive current_tick from the wall clock and run call_tick_tasks()
         * until it catches up, the same way the SDL sim's tick_timer() and
         * the ctru port do.
         *
         * Advancing it once per pass through this loop instead makes
         * current_tick mean "how many times this thread got to run", which is
         * always slower than real time: each pass costs the sleep plus the
         * handler plus however long sim_enter_irq_handler() spent waiting on
         * a thread that holds disable_irq(). Ticks lost that way were never
         * made up, so the whole of Rockbox's tick-counted timing ran slow by
         * an amount that grew with load. Input took the brunt of it, since
         * button_tick() is a tick task and button.c counts repeat and
         * long-press thresholds in ticks. */
        new_tick = (long)((sceKernelGetSystemTimeWide() - tick_origin_us) /
                          (1000000 / HZ));

        while (new_tick != current_tick)
        {
            sim_enter_irq_handler();
            call_tick_tasks();
            sim_exit_irq_handler();
        }
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

    irqlock_init(&irq_mtx);
    irq_cond_sema = sceKernelCreateSema("rb_irq_cond", 0, 0, 255, NULL);

    tick_origin_us = sceKernelGetSystemTimeWide();
    start_tick = sceKernelGetSystemTimeLow() / 1000;

    tick_thread_should_run = true;
    tick_thread_id = sceKernelCreateThread("rb_tick", tick_thread_func,
                                           0x12, 16*1024, 0, NULL);
    if (tick_thread_id >= 0)
        sceKernelStartThread(tick_thread_id, 0, NULL);
}
