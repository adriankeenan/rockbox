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

#include <pspkernel.h>
#include <pspthreadman.h>

#include "timer.h"

static int timer_prio = -1;
static void (*timer_callback)(void);
static volatile bool timer_run;
static long timer_period_us;
static SceUID timer_thread_id = -1;

static int timer_thread_func(SceSize args, void *argp)
{
    (void)args; (void)argp;

    while (timer_run)
    {
        sceKernelDelayThread(timer_period_us);
        if (timer_run && timer_callback)
            timer_callback();
    }

    return 0;
}

static void start_timer_thread(void)
{
    timer_run = true;
    timer_thread_id = sceKernelCreateThread("rb_timer", timer_thread_func,
                                            0x12, 16*1024, 0, NULL);
    if (timer_thread_id >= 0)
        sceKernelStartThread(timer_thread_id, 0, NULL);
}

static void stop_timer_thread(void)
{
    timer_run = false;
    if (timer_thread_id >= 0)
    {
        sceKernelWaitThreadEnd(timer_thread_id, NULL);
        sceKernelDeleteThread(timer_thread_id);
        timer_thread_id = -1;
    }
}

bool timer_register(int reg_prio, void (*unregister_callback)(void),
                    long cycles, void (*callback)(void))
{
    (void)unregister_callback;
    if (reg_prio <= timer_prio || cycles == 0)
        return false;

    stop_timer_thread();

    timer_prio = reg_prio;
    timer_callback = callback;
    timer_period_us = cycles;

    start_timer_thread();
    return true;
}

bool timer_set_period(long cycles)
{
    stop_timer_thread();
    timer_period_us = cycles;
    start_timer_thread();
    return true;
}

void timer_unregister(void)
{
    stop_timer_thread();
    timer_prio = -1;
}
