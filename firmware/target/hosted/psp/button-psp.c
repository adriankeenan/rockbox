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

#include <pspctrl.h>

#include "config.h"
#include "button.h"
#include "button-psp.h"
#include "kernel.h"
#include "debug.h"

static bool hold_state = false;

int button_read_device(int *data)
{
    (void)data;
    SceCtrlData pad;

    /* Peek, not Read: Read blocks until the next sampling cycle if called
     * more than once per cycle, which button_tick() (called every HZ
     * tick) reliably does. That block happens while this thread still
     * holds the cooperative scheduler's global lock (see thread-psp.c),
     * stalling every other Rockbox thread until the next sample -- Peek
     * just returns the latest sample immediately, which is what a
     * poll-driven caller like button_tick() actually wants. */
    if (sceCtrlPeekBufferPositive(&pad, 1) <= 0)
        return BUTTON_NONE;

    hold_state = (pad.Buttons & PSP_CTRL_HOLD) != 0;

    int key = BUTTON_NONE;

    if (pad.Buttons & PSP_CTRL_UP)       key |= BUTTON_UP;
    if (pad.Buttons & PSP_CTRL_DOWN)     key |= BUTTON_DOWN;
    if (pad.Buttons & PSP_CTRL_LEFT)     key |= BUTTON_LEFT;
    if (pad.Buttons & PSP_CTRL_RIGHT)    key |= BUTTON_RIGHT;
    if (pad.Buttons & PSP_CTRL_TRIANGLE) key |= BUTTON_TRIANGLE;
    if (pad.Buttons & PSP_CTRL_CIRCLE)   key |= BUTTON_CIRCLE;
    if (pad.Buttons & PSP_CTRL_CROSS)    key |= BUTTON_CROSS;
    if (pad.Buttons & PSP_CTRL_SQUARE)   key |= BUTTON_SQUARE;
    if (pad.Buttons & PSP_CTRL_LTRIGGER) key |= BUTTON_L;
    if (pad.Buttons & PSP_CTRL_RTRIGGER) key |= BUTTON_R;
    if (pad.Buttons & PSP_CTRL_START)    key |= BUTTON_START;
    if (pad.Buttons & PSP_CTRL_SELECT)   key |= BUTTON_SELECT;
    if (pad.Buttons & PSP_CTRL_HOME)     key |= BUTTON_HOME;

    return key;
}

void button_init_device(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
}

bool button_hold(void)
{
    return hold_state;
}
