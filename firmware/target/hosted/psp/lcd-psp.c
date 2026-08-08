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

/* Rockbox's internal framebuffer is LCD_WIDTH x LCD_HEIGHT (320x240, for
 * theme compatibility) but the real panel is PSP_DISP_WIDTH x
 * PSP_DISP_HEIGHT (480x272). Every update scales the whole 320x240 image
 * up into a centered, aspect-correct rectangle within the 480x272
 * output (pillarboxed left/right, since 4:3 is narrower than the
 * panel's ~1.76:1) using nearest-neighbour sampling. The scale factor is
 * non-integral (~1.13x), so a source pixel doesn't map cleanly onto a
 * fixed set of output pixels -- that makes correctly redrawing only a
 * sub-rectangle fragile, so lcd_update_rect always redraws the full
 * scaled frame rather than trying to clip the scaled blit.
 * scale_fp/colmap are computed once by lcd_init_device. */
static int scale_fp;   /* 16.16 fixed-point PSP_DISP/LCD scale factor */
static int scaled_w, scaled_h; /* size of the scaled image, in PSP_DISP pixels */
static int off_x, off_y;       /* top-left of the scaled image, centered */
static unsigned short colmap[PSP_DISP_WIDTH]; /* dest column -> source column */

static void lcd_scale_blit(void)
{
    for (int dy = 0; dy < scaled_h; dy++)
    {
        int sy = (dy << 16) / scale_fp;
        if (sy >= LCD_HEIGHT)
            sy = LCD_HEIGHT - 1;

        fb_data *dst = LCD_FRAMEBUF_ADDR(off_x, off_y + dy);
        const fb_data *srcrow = FBADDR(0, sy);

        for (int dx = 0; dx < scaled_w; dx++)
            dst[dx] = srcrow[colmap[dx]];
    }
}

void lcd_update_rect(int x, int y, int width, int height)
{
    (void)x; (void)y; (void)width; (void)height;

    if (dev_fb == NULL)
        return;

    lcd_scale_blit();

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

    int scale_x_fp = (PSP_DISP_WIDTH  << 16) / LCD_WIDTH;
    int scale_y_fp = (PSP_DISP_HEIGHT << 16) / LCD_HEIGHT;
    scale_fp = MIN(scale_x_fp, scale_y_fp);

    scaled_w = (LCD_WIDTH  * scale_fp) >> 16;
    scaled_h = (LCD_HEIGHT * scale_fp) >> 16;
    off_x = (PSP_DISP_WIDTH  - scaled_w) / 2;
    off_y = (PSP_DISP_HEIGHT - scaled_h) / 2;

    for (int dx = 0; dx < scaled_w; dx++)
    {
        int sx = (dx << 16) / scale_fp;
        colmap[dx] = (sx >= LCD_WIDTH) ? LCD_WIDTH - 1 : sx;
    }

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
