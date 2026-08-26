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
#include <pspthreadman.h>

#include "config.h"
#include "button.h"
#include "button-psp.h"
#include "analog-psp.h"
#include "kernel.h"
#include "debug.h"

static bool hold_state = false;

/* Analog stick: Lx/Ly are 0-255, resting near the middle. Tilting past the
 * threshold synthesizes the same BUTTON_UP/DOWN/LEFT/RIGHT bits as the
 * D-pad, so the stick works as an alternate input with no keymap
 * changes -- either the D-pad or the stick (or both at once) drives
 * the same UP/DOWN/LEFT/RIGHT actions.
 *
 * The thresholds are hysteretic and the centre is measured at init; see
 * analog-psp.h for why a bare comparison against a fixed 128 is not good
 * enough here. */
static int analog_center_x = PSP_ANALOG_CENTER;
static int analog_center_y = PSP_ANALOG_CENTER;
static int analog_state_x = 0;
static int analog_state_y = 0;

/* Last mask handed back, so a failed peek can repeat it instead of
 * reporting a full release. */
static int last_key = BUTTON_NONE;
static int peek_failures = 0;
/* How many consecutive failures to paper over before giving up. */
#define MAX_PEEK_FAILURES 3

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
    {
        /* A failed peek is not the same as "everything released", but
         * returning BUTTON_NONE makes button.c see one: it diffs against the
         * previous mask and posts BUTTON_REL for every bit that vanished.
         * Every confirm/cancel binding in keymap-psp.c is a |BUTTON_REL
         * entry, so that is an unrequested OK or CANCEL. Repeat the last
         * mask instead -- but only briefly, so a persistent failure releases
         * the buttons rather than latching them down forever. */
        if (++peek_failures <= MAX_PEEK_FAILURES)
            return last_key;

        analog_state_x = 0;
        analog_state_y = 0;
        last_key = BUTTON_NONE;
        return BUTTON_NONE;
    }

    peek_failures = 0;

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

    /* Ly grows downward on the PSP, so negative deflection is up. */
    int dx = psp_axis_direction((int)pad.Lx - analog_center_x, &analog_state_x);
    int dy = psp_axis_direction((int)pad.Ly - analog_center_y, &analog_state_y);

    if (dx < 0) key |= BUTTON_LEFT;
    if (dx > 0) key |= BUTTON_RIGHT;
    if (dy < 0) key |= BUTTON_UP;
    if (dy > 0) key |= BUTTON_DOWN;

    last_key = key;
    return key;
}

void button_init_device(void)
{
    int sum_x = 0, sum_y = 0, samples = 0;
    int i;

    sceCtrlSetSamplingCycle(0);
    /* ANALOG (not DIGITAL) mode: DIGITAL mode leaves Lx/Ly unpopulated. */
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    /* Measure where this nub actually rests instead of assuming 128 -- they
     * are commonly off-centre, and an off-centre nub sits closer to one
     * threshold than the other. psp_axis_calibrate() bounds how far this can
     * move the centre, so a nub held over at boot degrades rather than
     * calibrating to nonsense.
     *
     * One sleep, not one per sample: this runs on the thread holding the
     * cooperative scheduler lock (see thread-psp.c), so time spent blocked
     * here is time no other Rockbox thread runs. Sampling is VSync-locked
     * (~60Hz) at sampling cycle 0, so a single wait of over one frame is
     * enough for the mode switch above to have produced a real ANALOG
     * sample; the reads after it are free. */
    sceKernelDelayThread(20000);

    for (i = 0; i < 4; i++)
    {
        SceCtrlData pad;

        if (sceCtrlPeekBufferPositive(&pad, 1) <= 0)
            continue;

        /* Lx == Ly == 0 is the signature of a sample taken before ANALOG
         * mode took effect (DIGITAL mode leaves both unpopulated), not a
         * nub jammed into a corner -- averaging it in would drag the centre
         * to the clamp limit and bias every direction for the whole
         * session. */
        if (pad.Lx == 0 && pad.Ly == 0)
            continue;

        sum_x += pad.Lx;
        sum_y += pad.Ly;
        samples++;
    }

    if (samples > 0)
    {
        analog_center_x = psp_axis_calibrate(sum_x / samples);
        analog_center_y = psp_axis_calibrate(sum_y / samples);
    }
}

bool button_hold(void)
{
    return hold_state;
}
