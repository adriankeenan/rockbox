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

/* The generic hosted RTC (firmware/target/hosted/rtc.c) reads the clock with
 * time() + localtime(). On the PSP that does not reach the console's clock:
 * time() comes back at the epoch, localtime() hands Rockbox 1970, and
 * valid_time() (firmware/common/timefuncs.c) rejects anything before the year
 * 2000 -- so every clock display falls back to "--:--:--" and the RTC looks
 * absent even though CONFIG_RTC is enabled.
 *
 * Ask the SDK instead. -lpsprtc was already being linked for this and had no
 * caller until now. */

#include <string.h>
#include <psprtc.h>

#include "config.h"
#include "rtc.h"
#include "timefuncs.h"

void rtc_init(void)
{
}

int rtc_read_datetime(struct tm *tm)
{
    ScePspDateTime t;

    /* Local time, not UTC. The PSP holds a timezone in its system settings
     * but does not expose it to newlib, so there is no TZ for localtime() to
     * apply -- asking for local time directly is what keeps the displayed
     * clock matching the XMB's. */
    if (sceRtcGetCurrentClockLocalTime(&t) < 0)
    {
        /* get_time() ignores this return value, so the struct itself has to
         * say "no time". Zeroed tm_year is 1900, which is below
         * valid_time()'s year-2000 floor, giving "--:--:--" rather than a
         * plausible-looking wrong date. */
        memset(tm, 0, sizeof(*tm));
        return -1;
    }

    tm->tm_year = t.year - 1900;
    tm->tm_mon  = t.month - 1;
    tm->tm_mday = t.day;
    tm->tm_hour = t.hour;
    tm->tm_min  = t.minute;
    tm->tm_sec  = t.second;

    /* sceRtc gives no weekday or day-of-year, and valid_time() range-checks
     * tm_wday -- leaving it uninitialised would fail the very check this
     * driver exists to satisfy. */
    set_day_of_week(tm);
    set_day_of_year(tm);

    return 0;
}

int rtc_write_datetime(const struct tm *tm)
{
    /* Not supported: setting the console clock needs kernel mode and this
     * module runs as THREAD_ATTR_USER (see system-psp.c). Set the clock from
     * the XMB instead.
     *
     * Still returns success, matching what the generic hosted driver already
     * did for PSP -- time_menu.c ignores the result either way, and the next
     * read simply shows the console's unchanged time. */
    (void)tm;
    return 0;
}
