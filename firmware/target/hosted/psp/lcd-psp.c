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
#include <string.h>
#include <malloc.h>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <psputils.h>

#include "debug.h"
#include "system.h"
#include "lcd-target.h"

fb_data *dev_fb = NULL;

void lcd_update_rect(int x, int y, int width, int height)
{
    if (dev_fb == NULL)
        return;

    if (x + width > LCD_WIDTH)
        width = LCD_WIDTH - x;
    if (x < 0)
        width += x, x = 0;
    if (width <= 0)
        return;

    if (y + height > LCD_HEIGHT)
        height = LCD_HEIGHT - y;
    if (y < 0)
        height += y, y = 0;
    if (height <= 0)
        return;

    for (int row = 0; row < height; row++)
    {
        fb_data *dst = LCD_FRAMEBUF_ADDR(x, y + row);
        fb_data *src = FBADDR(x, y + row);
        memcpy(dst, src, width * sizeof(fb_data));
    }

    sceKernelDcacheWritebackInvalidateRange(dev_fb,
        PSP_SCR_STRIDE * LCD_HEIGHT * sizeof(fb_data));
    sceDisplaySetFrameBuf(dev_fb, PSP_SCR_STRIDE,
                          PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
}

void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

void lcd_init_device(void)
{
    size_t bufsize = PSP_SCR_STRIDE * LCD_HEIGHT * sizeof(fb_data);
    dev_fb = (fb_data *) memalign(64, bufsize);
    if (dev_fb == NULL)
    {
        DEBUGF("lcd_init_device: could not allocate framebuffer\n");
        exit(EXIT_FAILURE);
    }

    memset(dev_fb, 0, bufsize);

    sceDisplaySetMode(0, LCD_WIDTH, LCD_HEIGHT);
    sceDisplaySetFrameBuf(dev_fb, PSP_SCR_STRIDE,
                          PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_IMMEDIATE);
}

void lcd_shutdown(void)
{
    if (dev_fb)
    {
        free(dev_fb);
        dev_fb = NULL;
    }
}
