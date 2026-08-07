#ifndef LCD_TARGET_H
#define LCD_TARGET_H

#include "lcd.h"

/* Framebuffer device shown to the PSP display hardware. It uses a wider
 * stride (PSP_SCR_STRIDE) than the visible LCD_WIDTH because the display
 * controller wants a power-of-two-friendly buffer stride. */
#define PSP_SCR_STRIDE 512

extern fb_data *dev_fb;
#define LCD_FRAMEBUF_ADDR(col, row) (dev_fb + (row) * PSP_SCR_STRIDE + (col))

#endif /* LCD_TARGET_H */
