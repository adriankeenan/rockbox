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

/* Host-side test for the analog nub logic in analog-psp.h.
 *
 * Not part of any build -- firmware/SOURCES lists its files explicitly, so
 * this is inert there. analog-psp.h has no PSP dependencies, so run it with
 * the host compiler, from the repository root:
 *
 *   gcc -std=gnu11 -W -Wall -Wextra -Ifirmware/target/hosted/psp \
 *       -o /tmp/analog-psp-test \
 *       firmware/target/hosted/psp/analog-psp-test.c && /tmp/analog-psp-test
 *
 * Exits non-zero on failure.
 */

#include <stdio.h>
#include "app/button-target.h"
#include "analog-psp.h"

static int fails = 0;
static void check(const char *what, int got, int want)
{
    if (got != want) { printf("  FAIL %-52s got %d want %d\n", what, got, want); fails++; }
    else               printf("  ok   %-52s (%d)\n", what, got);
}

/* Count direction changes produced by a signal, the way button.c would see
   them as press/release events. */
static int transitions(const int *v, int n, int start_state)
{
    int state = start_state, prev = start_state, t = 0, i;
    for (i = 0; i < n; i++) {
        int d = psp_axis_direction(v[i], &state);
        if (d != prev) t++;
        prev = d;
    }
    return t;
}

/* What the ORIGINAL code did: bare comparison, no state. */
static int old_transitions(const int *v, int n)
{
    int prev = 0, t = 0, i;
    for (i = 0; i < n; i++) {
        int d = 0;
        if (v[i] >  64) d =  1;
        if (v[i] < -64) d = -1;
        if (d != prev) t++;
        prev = d;
    }
    return t;
}

int main(void)
{
    int i;

    /* 1. Nub resting ON the old threshold, jittering 60..68. This is the
          regression: the old code chatters, the new one must not move. */
    int boundary[40];
    for (i = 0; i < 40; i++) boundary[i] = (i % 2) ? 68 : 60;

    printf("held on the threshold (60..68 jitter, 40 samples):\n");
    check("old code transitions (the bug)", old_transitions(boundary, 40), 39);
    /* Crossing ENTER once and latching is correct for a nub the user is
       actually holding there; the bug is chattering every sample. */
    check("new code transitions",           transitions(boundary, 40, 0), 1);

    /* 2. Deliberate deflection: rest -> +100 -> rest. Exactly one assert,
          one release. */
    int deflect[60];
    for (i = 0; i < 20; i++) deflect[i] = 0;
    for (i = 20; i < 40; i++) deflect[i] = 100;
    for (i = 40; i < 60; i++) deflect[i] = 0;
    printf("clean deflection to +100 and back:\n");
    check("transitions", transitions(deflect, 60, 0), 2);

    /* 3. Off-centre rest inside ENTER but above LEAVE: must never assert
          from a centred start. */
    int offcentre[40];
    for (i = 0; i < 40; i++) offcentre[i] = (i % 2) ? 48 : 42;
    printf("off-centre rest (42..48, between LEAVE and ENTER):\n");
    check("transitions from centred start", transitions(offcentre, 40, 0), 0);

    /* 4. Once asserted, it holds until deflection drops below LEAVE. */
    {
        int state = 0;
        printf("latching behaviour:\n");
        check("asserts at +65",            psp_axis_direction(65, &state),  1);
        check("holds at +50 (above LEAVE)",psp_axis_direction(50, &state),  1);
        check("holds at +40 (== LEAVE)",   psp_axis_direction(40, &state),  1);
        check("releases at +39",           psp_axis_direction(39, &state),  0);
        check("does not re-assert at +39", psp_axis_direction(39, &state),  0);
    }

    /* 5. A hard flick through centre to the far side flips in one call. */
    {
        int state = 0;
        psp_axis_direction(100, &state);
        printf("hard flick across centre:\n");
        check("+100 -> -100 flips immediately", psp_axis_direction(-100, &state), -1);
    }

    /* 6. Calibration clamp. */
    printf("calibration clamp (nominal 128, max adj 32):\n");
    check("centred 128 passes through", psp_axis_calibrate(128), 128);
    check("mild offset 150 passes",     psp_axis_calibrate(150), 150);
    check("held hard left (0) clamps",  psp_axis_calibrate(0),    96);
    check("held hard right (255) clamps",psp_axis_calibrate(255),160);

    /* 7. The real-world failure: a nub that RESTS off-centre. Hysteresis
          alone would latch a direction on permanently, which in a menu is a
          continuous CANCEL. Calibration is what has to save this, so test
          the whole raw-sample -> calibrated -> direction path. */
    {
        static const int drift_raw[] = { 160, 176, 192, 208 };
        unsigned int d;
        printf("nub RESTING off-centre (end-to-end, with calibration):\n");
        for (d = 0; d < sizeof(drift_raw)/sizeof(drift_raw[0]); d++)
        {
            int raw = drift_raw[d];
            int centre = psp_axis_calibrate(raw);
            int state = 0, t = 0, prev = 0, k;
            char label[64];
            /* rest there, jittering +-4, for 100 polls */
            for (k = 0; k < 100; k++)
            {
                int v = raw + ((k % 2) ? 4 : -4) - centre;
                int dir = psp_axis_direction(v, &state);
                if (dir != prev) t++;
                prev = dir;
            }
            snprintf(label, sizeof label,
                     "raw rest %d -> centre %d: spurious events", raw, centre);
            check(label, t, 0);
        }
    }

    /* 8. Diagonal collapse. The reported bug: an off-centre D-pad press
          closes two switches, the resulting two-direction mask matches no
          keymap entry at all, and the press vanishes. */
    {
        int held = PSP_AXIS_NONE;
        printf("diagonal collapse (D-pad rocker):\n");
        check("UP alone passes through",
              psp_collapse_axis(BUTTON_UP, &held), BUTTON_UP);
        check("stray LEFT arriving mid-press is dropped",
              psp_collapse_axis(BUTTON_UP | BUTTON_LEFT, &held), BUTTON_UP);
        check("...and stays dropped while held",
              psp_collapse_axis(BUTTON_UP | BUTTON_LEFT, &held), BUTTON_UP);
        /* The back-a-level case: vertical opens first, leaving a bare LEFT
           that would otherwise post as ACTION_STD_CANCEL. */
        check("lone LEFT after UP releases is suppressed",
              psp_collapse_axis(BUTTON_LEFT, &held), 0);
        check("release clears the latch",
              psp_collapse_axis(0, &held), 0);
        check("LEFT alone now works again",
              psp_collapse_axis(BUTTON_LEFT, &held), BUTTON_LEFT);
        psp_collapse_axis(0, &held);
    }

    /* 9. A simultaneous tie resolves to vertical, and horizontal presses are
          not broken by the collapse. */
    {
        int held = PSP_AXIS_NONE;
        printf("ties and horizontal presses:\n");
        check("UP+LEFT in one poll -> vertical wins",
              psp_collapse_axis(BUTTON_UP | BUTTON_LEFT, &held), BUTTON_UP);
        psp_collapse_axis(0, &held);
        check("LEFT then stray UP -> horizontal keeps it",
              psp_collapse_axis(BUTTON_LEFT, &held), BUTTON_LEFT);
        check("   stray UP dropped",
              psp_collapse_axis(BUTTON_LEFT | BUTTON_UP, &held), BUTTON_LEFT);
        psp_collapse_axis(0, &held);
        check("opposing UP+DOWN is nonsense -> dropped",
              psp_collapse_axis(BUTTON_UP | BUTTON_DOWN, &held), 0);
    }

    /* 10. The D-pad must stay usable even while the nub holds a direction --
           the reason the two sources get separate latches. */
    {
        int dpad_held = PSP_AXIS_NONE, nub_held = PSP_AXIS_NONE;
        int nub = BUTTON_LEFT;   /* nub resting off-centre, latched */
        printf("nub stuck LEFT must not lock the D-pad out of vertical:\n");
        int k = psp_collapse_axis(BUTTON_UP, &dpad_held)
              | psp_collapse_axis(nub, &nub_held);
        check("D-pad UP still reaches the mask", (k & BUTTON_UP) != 0, 1);
        k = psp_collapse_axis(BUTTON_DOWN, &dpad_held)
          | psp_collapse_axis(nub, &nub_held);
        check("D-pad DOWN still reaches the mask", (k & BUTTON_DOWN) != 0, 1);
    }

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED",
           fails, fails == 1 ? "" : "s");
    return fails != 0;
}
