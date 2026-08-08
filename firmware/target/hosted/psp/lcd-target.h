#ifndef LCD_TARGET_H
#define LCD_TARGET_H

#include "lcd.h"

/* The real, physical PSP display panel -- distinct from LCD_WIDTH/
 * LCD_HEIGHT (320x240), which is Rockbox's internal/theme resolution.
 * lcd-psp.c scales the 320x240 software framebuffer up to fill this. */
#define PSP_DISP_WIDTH  480
#define PSP_DISP_HEIGHT 272

/* Framebuffer device shown to the PSP display hardware. It uses a wider
 * stride (PSP_SCR_STRIDE) than the visible PSP_DISP_WIDTH because the
 * display controller wants a power-of-two-friendly buffer stride. */
#define PSP_SCR_STRIDE 512

extern fb_data *dev_fb;
#define LCD_FRAMEBUF_ADDR(col, row) (dev_fb + (row) * PSP_SCR_STRIDE + (col))

#endif /* LCD_TARGET_H */
