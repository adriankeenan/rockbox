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
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#include <pspkernel.h>
#include <pspdisplay.h>
#include <psputils.h>

#include "debug.h"
#include "system.h"
#include "lcd-target.h"

fb_data *dev_fb = NULL;

/* Rockbox's internal framebuffer is LCD_WIDTH x LCD_HEIGHT (320x240, for
 * theme compatibility) but the real panel is PSP_DISP_WIDTH x
 * PSP_DISP_HEIGHT (480x272). Rendered 1:1 (no scaling), centered within
 * the panel with black borders on all sides. */
#define PSP_DISP_OFF_X ((PSP_DISP_WIDTH  - LCD_WIDTH)  / 2)
#define PSP_DISP_OFF_Y ((PSP_DISP_HEIGHT - LCD_HEIGHT) / 2)

/* Rockbox's RGB565 keeps red in the high five bits (pure red is 0xf800, see
 * _RGBPACK_LCD in firmware/export/lcd.h), but the PSP display controller's
 * 16-bit mode uses the hardware's native channel order -- red in the low five
 * bits, blue in the high five, matching GU_PSM_5650 and the ABGR8888 ordering
 * PSPSDK uses everywhere else. Handed Rockbox pixels unaltered, the panel
 * therefore shows every red as blue and vice versa; green sits in the middle
 * six bits either way, and greys have equal red and blue, which is why only
 * those two channels ever looked wrong.
 *
 * Note this is a channel swap, not the byte swap LCD_PIXELFORMAT's
 * RGB565SWAPPED describes, so there is no pixel format to select our way out
 * of it. Convert here, at the device framebuffer, and leave the rest of
 * Rockbox -- themes, bmp2rb's build-time native bitmaps, album art -- in
 * ordinary RGB565. */
static inline fb_data psp_swap_rb(fb_data p)
{
    return (p >> 11) | (p & 0x07e0) | ((p & 0x001f) << 11);
}

/* Two pixels at a time. Which half holds which pixel doesn't matter, both get
 * the same treatment. */
static inline uint32_t psp_swap_rb2(uint32_t w)
{
    return ((w & 0xf800f800u) >> 11) |
            (w & 0x07e007e0u)        |
           ((w & 0x001f001fu) << 11);
}

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
        fb_data *dst = LCD_FRAMEBUF_ADDR(PSP_DISP_OFF_X + x, PSP_DISP_OFF_Y + y + row);
        const fb_data *src = FBADDR(x, y + row);
        int n = width;

        /* Allegrex faults on unaligned word access, and src is whatever buffer
         * the current viewport points at, so only take the two-at-a-time path
         * when both sides agree on word alignment. They normally do: dev_fb is
         * memalign()ed, both strides are even, and PSP_DISP_OFF_X is 80. */
        if ((((uintptr_t)dst ^ (uintptr_t)src) & 2) == 0)
        {
            if (n > 0 && ((uintptr_t)dst & 2))
            {
                *dst++ = psp_swap_rb(*src++);
                n--;
            }

            uint32_t *d32 = (uint32_t *)dst;
            const uint32_t *s32 = (const uint32_t *)src;
            for (int i = n >> 1; i > 0; i--)
                *d32++ = psp_swap_rb2(*s32++);

            if (n & 1)
                *(fb_data *)d32 = psp_swap_rb(*(const fb_data *)s32);
        }
        else
        {
            for (int i = 0; i < n; i++)
                dst[i] = psp_swap_rb(src[i]);
        }
    }

    sceKernelDcacheWritebackInvalidateRange(dev_fb,
        PSP_SCR_STRIDE * PSP_DISP_HEIGHT * sizeof(fb_data));
    sceDisplaySetFrameBuf(dev_fb, PSP_SCR_STRIDE,
                          PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
}

void lcd_update(void)
{
    lcd_update_rect(0, 0, LCD_WIDTH, LCD_HEIGHT);
}

void lcd_init_device(void)
{
    size_t bufsize = PSP_SCR_STRIDE * PSP_DISP_HEIGHT * sizeof(fb_data);
    dev_fb = (fb_data *) memalign(64, bufsize);
    if (dev_fb == NULL)
    {
        DEBUGF("lcd_init_device: could not allocate framebuffer\n");
        exit(EXIT_FAILURE);
    }

    memset(dev_fb, 0, bufsize);

    sceDisplaySetMode(0, PSP_DISP_WIDTH, PSP_DISP_HEIGHT);
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
