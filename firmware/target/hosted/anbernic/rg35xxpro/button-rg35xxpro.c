/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Adrian Keenan
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

#include <SDL.h>
#include "config.h"
#include "button.h"
#include "button-target.h"

/* KNULLI presents the RG35XX Pro's built-in controls as an evdev gamepad, so
   all of the real input arrives through the SDL joystick events below.  The
   keyboard map is kept for running with a USB keyboard attached. */

/* TEMPORARY: logs every raw input event reaching Rockbox (keysym, joystick
   button/hat/axis index) to work out the real hardware mapping -- e.g. in
   case a physical control such as volume arrives as a keyboard event
   rather than a joystick one, or doesn't reach Rockbox at all. Written
   relative to cwd, which rockbox.sh sets to the directory the binary lives
   in, so it lands at roms/ports/rockbox/joydebug.log on the SD card.
   Remove once the mapping below is confirmed on-device. */
#include <stdio.h>
#include <stdarg.h>

static void rg35xxpro_joy_debug(const char *fmt, ...)
{
    FILE *f = fopen("joydebug.log", "a");
    if (!f)
        return;

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

int key_to_button(int keyboard_key)
{
    rg35xxpro_joy_debug("keysym=%d (0x%x)\n", keyboard_key, keyboard_key);

    int new_btn = BUTTON_NONE;
    switch (keyboard_key)
    {
        case SDLK_UP:
            new_btn = BUTTON_UP;
            break;
        case SDLK_DOWN:
            new_btn = BUTTON_DOWN;
            break;
        case SDLK_LEFT:
            new_btn = BUTTON_LEFT;
            break;
        case SDLK_RIGHT:
            new_btn = BUTTON_RIGHT;
            break;
        case SDLK_RETURN:
        case SDLK_a:
            new_btn = BUTTON_A;
            break;
        case SDLK_ESCAPE:
        case SDLK_b:
            new_btn = BUTTON_B;
            break;
        case SDLK_x:
            new_btn = BUTTON_X;
            break;
        case SDLK_y:
            new_btn = BUTTON_Y;
            break;
        case SDLK_q:
            new_btn = BUTTON_L1;
            break;
        case SDLK_w:
            new_btn = BUTTON_R1;
            break;
        case SDLK_e:
            new_btn = BUTTON_L2;
            break;
        case SDLK_r:
            new_btn = BUTTON_R2;
            break;
        case SDLK_BACKSPACE:
            new_btn = BUTTON_SELECT;
            break;
        case SDLK_SPACE:
            new_btn = BUTTON_START;
            break;
        case SDLK_m:
            new_btn = BUTTON_MENU;
            break;
        case SDLK_f:
            new_btn = BUTTON_FN;
            break;
        case SDLK_EQUALS:
            new_btn = BUTTON_VOL_UP;
            break;
        case SDLK_MINUS:
            new_btn = BUTTON_VOL_DOWN;
            break;
        case SDLK_p:
            new_btn = BUTTON_POWER;
            break;
        default:
            break;
    }
    return new_btn;
}

#ifdef HAVE_SDL_JOYSTICK

/* Joystick button indices as reported for the H700 gamepad.  If a control
   comes out wrong on hardware, "sdl2-jstest --test 0" (or evtest) gives the
   real indices and only this table needs correcting. */
int joy_to_button(int joy_button)
{
    rg35xxpro_joy_debug("button index=%d\n", joy_button);

    int new_btn = BUTTON_NONE;
    switch (joy_button)
    {
        case 0:
            new_btn = BUTTON_A;
            break;
        case 1:
            new_btn = BUTTON_B;
            break;
        case 2:
            new_btn = BUTTON_X;
            break;
        case 3:
            new_btn = BUTTON_Y;
            break;
        case 4:
            new_btn = BUTTON_L1;
            break;
        case 5:
            new_btn = BUTTON_R1;
            break;
        case 6:
            new_btn = BUTTON_SELECT;
            break;
        case 7:
            new_btn = BUTTON_START;
            break;
        case 8:
            new_btn = BUTTON_MENU;
            break;
        case 9:
            new_btn = BUTTON_L2;
            break;
        case 10:
            new_btn = BUTTON_R2;
            break;
        case 11:
            new_btn = BUTTON_FN;
            break;
        case 12:
            new_btn = BUTTON_VOL_UP;
            break;
        case 13:
            new_btn = BUTTON_VOL_DOWN;
            break;
        default:
            break;
    }
    return new_btn;
}

/* The D-pad arrives as a hat.  Called with an SDL_HAT_* bitmask; the caller
   diffs successive values so returning the full set is correct here. */
int joy_hat_to_button(int hat_value)
{
    rg35xxpro_joy_debug("hat value=0x%02x\n", hat_value);

    int new_btn = BUTTON_NONE;

    if (hat_value & SDL_HAT_UP)
        new_btn |= BUTTON_UP;
    if (hat_value & SDL_HAT_DOWN)
        new_btn |= BUTTON_DOWN;
    if (hat_value & SDL_HAT_LEFT)
        new_btn |= BUTTON_LEFT;
    if (hat_value & SDL_HAT_RIGHT)
        new_btn |= BUTTON_RIGHT;

    return new_btn;
}

/* Analog stick: axis 0 is X, axis 1 is Y.  "positive" is 1 when the axis has
   moved past the deadzone in the positive direction, 0 for negative. */
int joy_axis_to_button(int axis, int positive)
{
    rg35xxpro_joy_debug("axis=%d positive=%d\n", axis, positive);

    int new_btn = BUTTON_NONE;
    switch (axis)
    {
        case 0:
            new_btn = positive ? BUTTON_RIGHT : BUTTON_LEFT;
            break;
        case 1:
            new_btn = positive ? BUTTON_DOWN : BUTTON_UP;
            break;
        default:
            break;
    }
    return new_btn;
}

#endif /* HAVE_SDL_JOYSTICK */
