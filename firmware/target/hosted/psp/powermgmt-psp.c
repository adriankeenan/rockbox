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

#include <stdbool.h>

#include <psppower.h>

#include "config.h"
#include "kernel.h"
#include "powermgmt.h"
#include "power.h"

unsigned short battery_level_disksafe = 0;
unsigned short battery_level_shutoff = 0;

/* voltages (millivolt) of 0%, 10%, ... 100%; PSP reports percentage
 * directly so these tables are unused but must exist. */
unsigned short percent_to_volt_discharge[11] =
{
};

unsigned short percent_to_volt_charge[11] =
{
};

unsigned int power_input_status(void)
{
    unsigned status = POWER_INPUT_NONE;
    if (scePowerIsPowerOnline())
        status = POWER_INPUT_MAIN_CHARGER;
    return status;
}

int _battery_level(void)
{
    int level = scePowerGetBatteryLifePercent();
    if (level < 0)
        level = 100; /* no battery info (e.g. running under an emulator) */
    return level;
}

bool charging_state(void)
{
    return scePowerIsBatteryCharging() != 0;
}
