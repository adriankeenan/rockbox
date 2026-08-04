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

#ifndef _BUTTON_TARGET_H_
#define _BUTTON_TARGET_H_

/* Main unit's buttons.  Bits 0..24 are free for target use; 0x02000000 and
   above are reserved for the BUTTON_REL/REPEAT/... modifiers in button.h */
#define BUTTON_UP       0x00000001
#define BUTTON_DOWN     0x00000002
#define BUTTON_LEFT     0x00000004
#define BUTTON_RIGHT    0x00000008
#define BUTTON_A        0x00000010
#define BUTTON_B        0x00000020
#define BUTTON_X        0x00000040
#define BUTTON_Y        0x00000080
#define BUTTON_L1       0x00000100
#define BUTTON_R1       0x00000200
#define BUTTON_L2       0x00000400
#define BUTTON_R2       0x00000800
#define BUTTON_SELECT   0x00001000
#define BUTTON_START    0x00002000
#define BUTTON_MENU     0x00004000
#define BUTTON_FN       0x00008000
#define BUTTON_VOL_UP   0x00010000
#define BUTTON_VOL_DOWN 0x00020000
#define BUTTON_POWER    0x00040000

#define BUTTON_MAIN     0x0007ffff

/* Software power-off */
#define POWEROFF_BUTTON BUTTON_POWER
#define POWEROFF_COUNT 10

/* SDL keyboard fallback; also what the simulator uses */
int key_to_button(int keyboard_key);

#ifdef HAVE_SDL_JOYSTICK
/* KNULLI exposes the built-in controls as an evdev gamepad, which SDL2
   reports as a joystick.  These map its events onto the bitmap above. */
int joy_to_button(int joy_button);
int joy_hat_to_button(int hat_value);
int joy_axis_to_button(int axis, int positive);
#endif

#endif /* _BUTTON_TARGET_H_ */
