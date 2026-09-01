/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2021 Aidan MacDonald
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

#ifndef BOOTLOADER
#include "system.h"
#include "kernel.h"
#include "button.h"
#include "lcd.h"
#include "font.h"
#include "action.h"
#include "list.h"

#include "clk-x1000.h"
#include "gpio-x1000.h"

#ifdef EROS_QN
#include "usb.h"
#include "power.h"
#include "devicedata.h"
#endif

static bool dbg_clocks(void)
{
    do {
        lcd_clear_display();
        int line = 0;
        for(int i = 0; i < X1000_CLK_COUNT; ++i) {
            uint32_t hz = clk_get(i);
            uint32_t khz = hz / 1000;
            uint32_t mhz = khz / 1000;
            lcd_putsf(2, line++, "%8s  %4u,%03u,%03u Hz", clk_get_name(i),
                      mhz, (khz - mhz*1000), (hz - khz*1000));
        }

        lcd_update();
    } while(get_action(CONTEXT_STD, HZ) != ACTION_STD_CANCEL);

    return false;
}

static void dbg_gpios_show_state(void)
{
    const char portname[] = "ABCD";
    for(int i = 0; i < 4; ++i)
        lcd_putsf(0, i, "GPIO %c: %08x", portname[i], REG_GPIO_PIN(i));
}

static void dbg_gpios_show_config(void)
{
    const char portname[] = "ABCD";
    int line = 0;
    for(int i = 0; i < 4; ++i) {
        uint32_t intr = REG_GPIO_INT(i);
        uint32_t mask = REG_GPIO_MSK(i);
        uint32_t pat0 = REG_GPIO_PAT0(i);
        uint32_t pat1 = REG_GPIO_PAT1(i);
        lcd_putsf(0, line++, "GPIO %c", portname[i]);
        lcd_putsf(2, line++, " int %08lx", intr);
        lcd_putsf(2, line++, " msk %08lx", mask);
        lcd_putsf(2, line++, "pat0 %08lx", pat0);
        lcd_putsf(2, line++, "pat1 %08lx", pat1);
        line++;
    }
}

static bool dbg_gpios(void)
{
    enum { STATE, CONFIG, NUM_SCREENS };
    const int timeouts[NUM_SCREENS] = { 1, HZ };
    int screen = STATE;

    while(1) {
        lcd_clear_display();
        switch(screen) {
        case CONFIG:
            dbg_gpios_show_config();
            break;
        case STATE:
            dbg_gpios_show_state();
            break;
        }

        lcd_update();

        switch(get_action(CONTEXT_STD, timeouts[screen])) {
        case ACTION_STD_CANCEL:
            return false;
        case ACTION_STD_PREV:
        case ACTION_STD_PREVREPEAT:
            screen -= 1;
            if(screen < 0)
                screen = NUM_SCREENS - 1;
            break;
        case ACTION_STD_NEXT:
        case ACTION_STD_NEXTREPEAT:
            screen += 1;
            if(screen >= NUM_SCREENS)
                screen = 0;
            break;
        default:
            break;
        }
    }

    return false;
}

#ifdef EROS_QN
/* Probe screen for the TCS1421 USB Type-C CC controller's mode pins.
 *
 * A Type-C host supplies no VBUS at all until the device presents Rd on CC,
 * so if the TCS1421 is left in the wrong role a C-to-C cable does nothing --
 * no data, and no charging either. An A-to-C cable supplies VBUS regardless
 * of CC state and so works in *every* role, which means it cannot be used to
 * tell the roles apart. Hence this screen: it cycles the mode pins and shows
 * the resulting attach state live, so a C-to-C cable can do the deciding.
 *
 * In the two input states the level readout shows what the board itself
 * straps the pin to, which tells us whether driving it at all is correct.
 *
 * CFG0 is only wired to the SoC on hw4+; on hw1-3 that pin is the back button
 * and the TCS1421 is strapped on the board, so it is left alone there.
 */
enum {
    TYPEC_OUT_LOW,
    TYPEC_OUT_HIGH,
    TYPEC_IN_PULL,
    TYPEC_IN_NOPULL,
    TYPEC_NUM_STATES,
};

static const char* const typec_state_names[TYPEC_NUM_STATES] = {
    "out 0", "out 1", "in PULL=1", "in PULL=0",
};

static void dbg_typec_apply(int gpio, int state)
{
    switch(state) {
    case TYPEC_OUT_LOW:
        gpio_set_function(gpio, GPIOF_OUTPUT(0));
        break;
    case TYPEC_OUT_HIGH:
        gpio_set_function(gpio, GPIOF_OUTPUT(1));
        break;
    case TYPEC_IN_PULL:
        gpio_set_function(gpio, GPIOF_INPUT);
        gpio_set_pull(gpio, 1);
        break;
    case TYPEC_IN_NOPULL:
        gpio_set_function(gpio, GPIOF_INPUT);
        gpio_set_pull(gpio, 0);
        break;
    }
}

static bool dbg_typec(void)
{
    int hw_rev = device_data.hw_rev;
    bool has_cfg0 = hw_rev >= 4;
    int cfg_gpio[2] = { GPIO_TCS1421_CFG1, GPIO_TCS1421_CFG0 };
    int state[2] = { TYPEC_OUT_LOW, TYPEC_OUT_LOW };
    int sel = 0;
    bool done = false;

    while(!done) {
        int line = 0;
        lcd_clear_display();
        lcd_putsf(0, line++, "TCS1421 Type-C probe");
        lcd_putsf(0, line++, "hw rev %d", hw_rev);
        line++;

        lcd_putsf(0, line++, "%c CFG1 PB30 %-9s lvl %d",
                  sel == 0 ? '>' : ' ', typec_state_names[state[0]],
                  gpio_get_level(GPIO_TCS1421_CFG1));
        if(has_cfg0)
            lcd_putsf(0, line++, "%c CFG0 PD5  %-9s lvl %d",
                      sel == 1 ? '>' : ' ', typec_state_names[state[1]],
                      gpio_get_level(GPIO_TCS1421_CFG0));
        else
            lcd_putsf(0, line++, "  CFG0 PD5  (hw4+ only)");
        line++;

        lcd_putsf(0, line++, "USB_DETECT PD3: %d",
                  gpio_get_level(GPIO_USB_DETECT));
        lcd_putsf(0, line++, "usb_detect():   %s",
                  usb_detect() == USB_INSERTED ? "INSERTED" : "EXTRACTED");
        lcd_putsf(0, line++, "charging:       %s",
                  charging_state() ? "yes" : "no");
        line++;

        lcd_putsf(0, line++, "NEXT/PREV: change state");
        if(has_cfg0)
            lcd_putsf(0, line++, "OK:        select pin");
        lcd_putsf(0, line++, "CANCEL:    exit");

        lcd_update();

        switch(get_action(CONTEXT_STD, HZ/2)) {
        case ACTION_STD_CANCEL:
            done = true;
            break;
        case ACTION_STD_OK:
            if(has_cfg0)
                sel ^= 1;
            break;
        case ACTION_STD_NEXT:
        case ACTION_STD_NEXTREPEAT:
            state[sel] = (state[sel] + 1) % TYPEC_NUM_STATES;
            dbg_typec_apply(cfg_gpio[sel], state[sel]);
            break;
        case ACTION_STD_PREV:
        case ACTION_STD_PREVREPEAT:
            state[sel] = (state[sel] + TYPEC_NUM_STATES - 1) % TYPEC_NUM_STATES;
            dbg_typec_apply(cfg_gpio[sel], state[sel]);
            break;
        default:
            break;
        }
    }

    /* Put the pins back the way gpio_init() left them */
    gpio_set_function(GPIO_TCS1421_CFG1, GPIOF_OUTPUT(0));
    if(has_cfg0)
        gpio_set_function(GPIO_TCS1421_CFG0, GPIOF_OUTPUT(0));

    return false;
}
#endif /* EROS_QN */

extern volatile unsigned aic_tx_underruns;
#ifdef HAVE_RECORDING
extern volatile unsigned aic_rx_overruns;
#endif
#ifdef HAVE_EROS_QN_CODEC
extern int es9018k2m_present_flag;
#endif

static bool dbg_audio(void)
{
    do {
        lcd_clear_display();
        lcd_putsf(0, 0, "TX underruns: %u", aic_tx_underruns);
#ifdef HAVE_RECORDING
        lcd_putsf(0, 1, "RX overruns:  %u", aic_rx_overruns);
#endif
#ifdef HAVE_EROS_QN_CODEC
        if (es9018k2m_present_flag)
        {
            lcd_putsf(0, 2, "(%d) ES9018K2M HWVOL", es9018k2m_present_flag);
        }
        else
        {
            lcd_putsf(0, 2, "(%d) SWVOL", es9018k2m_present_flag);
        }
#endif
        lcd_update();
    } while(get_action(CONTEXT_STD, HZ) != ACTION_STD_CANCEL);

    return false;
}

#ifdef X1000_CPUIDLE_STATS
static bool dbg_cpuidle(void)
{
    do {
        lcd_clear_display();
        lcd_putsf(0, 0, "CPU idle time: %d.%01d%%",
                  __cpu_idle_cur/10, __cpu_idle_cur%10);
        lcd_putsf(0, 1, "CPU frequency: %d.%03d MHz",
                  FREQ/1000000, (FREQ%1000000)/1000);
        lcd_update();
    } while(get_action(CONTEXT_STD, HZ) != ACTION_STD_CANCEL);

    return false;
}
#endif

#ifdef FIIO_M3K
extern bool dbg_fiiom3k_touchpad(void);
#endif
#ifdef SHANLING_Q1
extern bool dbg_shanlingq1_touchscreen(void);
#endif
#ifdef HAVE_AXP_PMU
extern bool axp_debug_menu(void);
#endif
#ifdef HAVE_CW2015
extern bool cw2015_debug_menu(void);
#endif

/* Menu definition */
static const struct {
    const char* name;
    bool(*function)(void);
} menuitems[] = {
    {"Clocks", &dbg_clocks},
    {"GPIOs", &dbg_gpios},
#ifdef EROS_QN
    {"Type-C CC", &dbg_typec},
#endif
#ifdef X1000_CPUIDLE_STATS
    {"CPU idle", &dbg_cpuidle},
#endif
    {"Audio", &dbg_audio},
#ifdef FIIO_M3K
    {"Touchpad", &dbg_fiiom3k_touchpad},
#endif
#ifdef SHANLING_Q1
    {"Touchscreen", &dbg_shanlingq1_touchscreen},
#endif
#ifdef HAVE_AXP_PMU
    {"Power stats", &axp_debug_menu},
#endif
#ifdef HAVE_CW2015
    {"CW2015 debug", &cw2015_debug_menu},
#endif
};

static int hw_info_menu_action_cb(int btn, struct gui_synclist* lists)
{
    if(btn == ACTION_STD_OK) {
        int sel = gui_synclist_get_sel_pos(lists);
        FOR_NB_SCREENS(i)
            viewportmanager_theme_enable(i, false, NULL);

        lcd_setfont(FONT_SYSFIXED);
        lcd_set_foreground(LCD_WHITE);
        lcd_set_background(LCD_BLACK);

        if(menuitems[sel].function())
            btn = SYS_USB_CONNECTED;
        else
            btn = ACTION_REDRAW;

        lcd_setfont(FONT_UI);

        FOR_NB_SCREENS(i)
            viewportmanager_theme_undo(i, false);
    }

    return btn;
}

static const char* hw_info_menu_get_name(int item, void* data,
                                         char* buffer, size_t buffer_len)
{
    (void)buffer;
    (void)buffer_len;
    (void)data;
    return menuitems[item].name;
}

bool dbg_hw_info(void)
{
    struct simplelist_info info;
    simplelist_info_init(&info, MODEL_NAME " debug menu",
                         ARRAYLEN(menuitems), NULL);
    info.action_callback = hw_info_menu_action_cb;
    info.get_name = hw_info_menu_get_name;
    return simplelist_show_list(&info);
}

bool dbg_ports(void)
{
    return false;
}
#endif
