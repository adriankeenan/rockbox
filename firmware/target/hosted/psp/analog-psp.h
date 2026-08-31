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

#ifndef _ANALOG_PSP_H_
#define _ANALOG_PSP_H_

/* Producing Rockbox direction buttons on the PSP: turning the analog nub into
 * them, and collapsing a two-direction press down to one.
 *
 * This lives in its own dependency-free header so the decision logic can be
 * unit-tested on the host without a PSP toolchain -- see the test referenced
 * from button-psp.c.
 *
 * The nub bits are OR'd into the same BUTTON_LEFT/RIGHT/UP/DOWN as the D-pad,
 * and in button_context_standard BUTTON_LEFT is a plain-press
 * ACTION_STD_CANCEL (apps/keymaps/keymap-psp.c). A single spurious LEFT is
 * therefore "back out of this menu", so the nub must not chatter: a bare
 * threshold comparison toggles freely while the nub rests near it, which is
 * exactly what a worn or off-centre nub does. Hence hysteresis, plus a
 * calibrated centre rather than assuming the nub rests at 128. */

/* Deflection from the calibrated centre needed to assert a direction. */
#define PSP_ANALOG_ENTER 64
/* The direction only releases once deflection falls back inside this. Must be
 * < PSP_ANALOG_ENTER: the gap between the two is the dead band that stops a
 * value sitting on the boundary from flipping the bit every poll. */
#define PSP_ANALOG_LEAVE 40

/* Assumed resting position of the nub on each axis (Lx/Ly are 0-255). */
#define PSP_ANALOG_CENTER 128
/* How far calibration may move the centre. Bounds the damage when the nub is
 * held over at boot: worst case one direction needs this many counts more
 * travel, rather than the centre landing somewhere useless. */
#define PSP_ANALOG_CENTER_MAX_ADJ 32

/* Latching hysteresis for one axis.
 *
 * v      signed deflection from the calibrated centre
 * state  -1/0/+1, persists across calls, owned by the caller
 *
 * Returns the new state. A hard flick straight through the centre to the
 * opposite extreme still flips in one call. */
static inline int psp_axis_direction(int v, int *state)
{
    if (*state > 0 && v < PSP_ANALOG_LEAVE)
        *state = 0;
    else if (*state < 0 && v > -PSP_ANALOG_LEAVE)
        *state = 0;

    if (*state == 0)
    {
        if (v > PSP_ANALOG_ENTER)
            *state = 1;
        else if (v < -PSP_ANALOG_ENTER)
            *state = -1;
    }

    return *state;
}

/* Clamp a measured resting position to within PSP_ANALOG_CENTER_MAX_ADJ of
 * the nominal centre. */
static inline int psp_axis_calibrate(int measured)
{
    if (measured < PSP_ANALOG_CENTER - PSP_ANALOG_CENTER_MAX_ADJ)
        return PSP_ANALOG_CENTER - PSP_ANALOG_CENTER_MAX_ADJ;
    if (measured > PSP_ANALOG_CENTER + PSP_ANALOG_CENTER_MAX_ADJ)
        return PSP_ANALOG_CENTER + PSP_ANALOG_CENTER_MAX_ADJ;
    return measured;
}

/* Which axis a press is being attributed to. */
#define PSP_AXIS_NONE 0
#define PSP_AXIS_VERT 1
#define PSP_AXIS_HORZ 2

/* Collapse a direction bitmap to a single axis.
 *
 * The PSP D-pad is a rocker: an off-centre press closes two switches at once,
 * which is exactly how games get diagonals. Rockbox has no diagonal binding
 * anywhere, and apps/action.c matches the whole button mask exactly
 * (action.c:574, against the full mask button.c posts), so a diagonal matches
 * nothing and the press is silently swallowed. Worse, the two switches rarely
 * open on the same tick, so the survivor gets posted alone -- and a bare
 * BUTTON_LEFT is ACTION_STD_CANCEL with no pre-button in
 * button_context_standard, i.e. a menu jumping back a level mid-scroll.
 *
 * Whichever axis opened the press keeps it until this source reports no
 * direction at all. *held is PSP_AXIS_*, owned by the caller. Callers must
 * have BUTTON_UP/DOWN/LEFT/RIGHT in scope.
 *
 * Opposing pairs (UP+DOWN) are dropped outright rather than picked between:
 * a rocker cannot really be both, so the reading is nonsense either way. */
static inline int psp_collapse_axis(int dirs, int *held)
{
    int vert = dirs & (BUTTON_UP | BUTTON_DOWN);
    int horz = dirs & (BUTTON_LEFT | BUTTON_RIGHT);

    if (vert == (BUTTON_UP | BUTTON_DOWN))
        vert = 0;
    if (horz == (BUTTON_LEFT | BUTTON_RIGHT))
        horz = 0;

    if (!vert && !horz)
    {
        *held = PSP_AXIS_NONE;
        return 0;
    }

    /* Both arriving in the same poll is a genuine tie -- there is no
     * magnitude to compare on a rocker. Prefer vertical: the menus this
     * mostly affects scroll that way. */
    if (*held == PSP_AXIS_NONE)
        *held = vert ? PSP_AXIS_VERT : PSP_AXIS_HORZ;

    return (*held == PSP_AXIS_VERT) ? vert : horz;
}

#endif /* _ANALOG_PSP_H_ */
