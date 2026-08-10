/***************************************************************************
 *             __________               __   ___
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

/* The backlight sysfs node name is not stable across KNULLI builds for the
   H700 family, so rather than hardcoding a path (as backlight-unix.c does for
   other targets) we discover it at init and scale Rockbox's brightness range
   onto whatever max_brightness the panel reports. */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "config.h"
#include "backlight-target.h"
#include "sysfs.h"

#define SYSFS_BACKLIGHT_DIR "/sys/class/backlight"
#define BL_PATH_MAX 128

/* Ref: https://www.kernel.org/doc/html/latest/gpu/backlight.html */
#define BACKLIGHT_POWER_ON      0
#define BACKLIGHT_POWER_REDUCED 1

static char sysfs_bl_brightness[BL_PATH_MAX];
static char sysfs_bl_power[BL_PATH_MAX];

/* Highest value the panel accepts; the Rockbox range is scaled onto it */
static int hw_max_brightness = MAX_BRIGHTNESS_SETTING;
static bool backlight_available = false;

static int last_bl = -1;

/* Find the first entry under /sys/class/backlight and cache its paths */
static bool find_backlight_device(void)
{
    DIR *dir = opendir(SYSFS_BACKLIGHT_DIR);
    if (!dir)
        return false;

    bool found = false;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(sysfs_bl_brightness, sizeof(sysfs_bl_brightness),
                 SYSFS_BACKLIGHT_DIR "/%s/brightness", entry->d_name);
        snprintf(sysfs_bl_power, sizeof(sysfs_bl_power),
                 SYSFS_BACKLIGHT_DIR "/%s/bl_power", entry->d_name);

        char max_path[BL_PATH_MAX];
        snprintf(max_path, sizeof(max_path),
                 SYSFS_BACKLIGHT_DIR "/%s/max_brightness", entry->d_name);

        int max;
        if (sysfs_get_int(max_path, &max) && max > 0)
            hw_max_brightness = max;

        found = true;
        break;
    }

    closedir(dir);
    return found;
}

/* Map MIN_BRIGHTNESS_SETTING..MAX_BRIGHTNESS_SETTING onto 1..hw_max */
static int scale_brightness(int brightness)
{
    const int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;

    if (range <= 0)
        return hw_max_brightness;

    int scaled = ((brightness - MIN_BRIGHTNESS_SETTING) * (hw_max_brightness - 1))
                 / range + 1;

    return scaled;
}

bool backlight_hw_init(void)
{
    backlight_available = find_backlight_device();
    if (!backlight_available)
        return false;

    backlight_hw_on();
    backlight_hw_brightness(DEFAULT_BRIGHTNESS_SETTING);

    return true;
}

void backlight_hw_on(void)
{
    if (backlight_available && last_bl != BACKLIGHT_POWER_ON)
    {
        last_bl = BACKLIGHT_POWER_ON;
        sysfs_set_int(sysfs_bl_power, last_bl);
    }
}

void backlight_hw_off(void)
{
    if (backlight_available && last_bl != BACKLIGHT_POWER_REDUCED)
    {
        last_bl = BACKLIGHT_POWER_REDUCED;
        sysfs_set_int(sysfs_bl_power, last_bl);
    }
}

void backlight_hw_brightness(int brightness)
{
    if (!backlight_available)
        return;

    /* cap range, just in case */
    if (brightness > MAX_BRIGHTNESS_SETTING)
        brightness = MAX_BRIGHTNESS_SETTING;
    if (brightness < MIN_BRIGHTNESS_SETTING)
        brightness = MIN_BRIGHTNESS_SETTING;

    sysfs_set_int(sysfs_bl_brightness, scale_brightness(brightness));
}
