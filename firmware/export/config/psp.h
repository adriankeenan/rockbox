/*
 * This config file is for the Sony PSP hosted application (homebrew EBOOT)
 */

/* We don't run on hardware directly */
#define CONFIG_PLATFORM (PLATFORM_HOSTED|PLATFORM_PSP)
#define HAVE_FPU

/* PSPSDK has no dlopen()-equivalent and psp-gcc's ABI can't generate PIC
 * code at all, ruling out the BINFMT_DLOPEN approach every other hosted
 * target uses. Codecs are instead linked at a fixed address (like native
 * targets) and loaded with a raw read() into memory -- see lc-psp.c and
 * the PSP branch in apps/plugins/plugin.lds. */
#define CONFIG_BINFMT BINFMT_ROCK

/* For Rolo and boot loader.  Allocated from the fork-reserved MODEL_NUMBER
 * range documented at the top of firmware/export/config.h -- this port isn't
 * upstreamed, so it will never be assigned a number by that process, and
 * anything near upstream's sequential range collides on the next merge. */
#define MODEL_NUMBER  10001
#define MODEL_NAME    "Sony PSP"

#define USB_NONE

#define CONFIG_CPU    PSP_ALLEGREX

#define CPU_FREQ      222000000

/* define this if you have a colour LCD */
#define HAVE_LCD_COLOR

/* define this if you want album art for this target */
#define HAVE_ALBUMART

/* define this to enable bitmap scaling */
#define HAVE_BMP_SCALING

/* define this to enable JPEG decoding */
#define HAVE_JPEG

/* define this if you have access to the quickscreen */
#define HAVE_QUICKSCREEN

/* define this if you would like tagcache to build on this target */
#define HAVE_TAGCACHE

/* Rockbox's internal software framebuffer is kept at the classic 320x240
 * resolution (rather than the PSP's native 480x272) so that the large
 * body of existing 320x240 Rockbox themes work unmodified. lcd-psp.c
 * scales this up to fill the real 480x272 panel at display-update time,
 * preserving aspect ratio (pillarboxed, since 320x240 is 4:3 vs. the
 * panel's wider ~1.76:1). See PSP_DISP_WIDTH/PSP_DISP_HEIGHT in
 * lcd-target.h for the real physical panel dimensions. */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

#define LCD_DEPTH  16
#define LCD_PIXELFORMAT RGB565

/* approximate DPI of the PSP's 4.3" screen as seen through the scaled-up
 * 320x240 logical framebuffer (240 logical rows fill the same ~2.4in of
 * panel height that 272 native rows used to) */
#define LCD_DPI 100

/* define this to indicate your device's keypad */
#define HAVE_BUTTON_DATA

/* define this if you have a real-time clock */
#define CONFIG_RTC RTC_HOSTED

/* Power management */
#define CONFIG_BATTERY_MEASURE PERCENTAGE_MEASURE
#define CONFIG_CHARGING        CHARGING_MONITOR
#define HAVE_SW_POWEROFF

/* The number of bytes reserved for loadable codecs */
#define CODEC_SIZE 0x100000

/* The number of bytes reserved for loadable plugins */
#define PLUGIN_BUFFER_SIZE 0x80000

#define AB_REPEAT_ENABLE

#define CONFIG_KEYPAD PSP_PAD

/* Define this for LCD backlight available */
#define HAVE_BACKLIGHT

#define CONFIG_LCD LCD_COWOND2

#define BOOTDIR "/PSP/GAME/ROCKBOX"

/* No special storage */
#define CONFIG_STORAGE STORAGE_HOSTFS
#define HAVE_STORAGE_FLUSH
