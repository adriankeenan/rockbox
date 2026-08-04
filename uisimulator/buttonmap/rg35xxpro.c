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
#include "button.h"
#include "buttonmap.h"

int key_to_button(int keyboard_button)
{
    int new_btn = BUTTON_NONE;
    switch (keyboard_button)
    {
        case SDLK_KP_4:
        case SDLK_LEFT:
            new_btn = BUTTON_LEFT;
            break;
        case SDLK_KP_6:
        case SDLK_RIGHT:
            new_btn = BUTTON_RIGHT;
            break;
        case SDLK_KP_8:
        case SDLK_UP:
            new_btn = BUTTON_UP;
            break;
        case SDLK_KP_2:
        case SDLK_DOWN:
            new_btn = BUTTON_DOWN;
            break;
        case SDLK_KP_5:
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
    }
    return new_btn;
}

struct button_map bm[] = {
    { SDLK_LEFT,        0, 0, 0, "Left" },
    { SDLK_RIGHT,       0, 0, 0, "Right" },
    { SDLK_UP,          0, 0, 0, "Up" },
    { SDLK_DOWN,        0, 0, 0, "Down" },
    { SDLK_RETURN,      0, 0, 0, "A" },
    { SDLK_b,           0, 0, 0, "B" },
    { SDLK_x,           0, 0, 0, "X" },
    { SDLK_y,           0, 0, 0, "Y" },
    { SDLK_q,           0, 0, 0, "L1" },
    { SDLK_w,           0, 0, 0, "R1" },
    { SDLK_e,           0, 0, 0, "L2" },
    { SDLK_r,           0, 0, 0, "R2" },
    { SDLK_BACKSPACE,   0, 0, 0, "Select" },
    { SDLK_SPACE,       0, 0, 0, "Start" },
    { SDLK_m,           0, 0, 0, "Menu" },
    { SDLK_f,           0, 0, 0, "Function" },
    { SDLK_EQUALS,      0, 0, 0, "Volume Up" },
    { SDLK_MINUS,       0, 0, 0, "Volume Down" },
    { SDLK_p,           0, 0, 0, "Power" },
    { 0, 0, 0, 0, "None" }
};
