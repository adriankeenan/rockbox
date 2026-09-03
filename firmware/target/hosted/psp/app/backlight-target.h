#ifndef _BACKLIGHT_TARGET_H_
#define _BACKLIGHT_TARGET_H_

#include <stdbool.h>

/* See backlight.c */
bool backlight_hw_init(void);
void backlight_hw_on(void);
void backlight_hw_off(void);
void backlight_hw_brightness(int brightness);

#endif
