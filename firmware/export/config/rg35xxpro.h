/*
 * This config file is for the Anbernic RG35XX Pro running the KNULLI CFW
 */

/* We run as an application on top of the KNULLI (Batocera) Linux userland.
   KNULLI mounts the user-writable SHARE partition at /userdata, which becomes
   the root of the filesystem as Rockbox sees it. */
#ifndef SIMULATOR
#define CONFIG_PLATFORM (PLATFORM_HOSTED)
#define PIVOT_ROOT "/userdata"
#endif

#define HAVE_FPU

/* For Rolo and boot loader.  Deliberately far above upstream's current
   sequential range (mid-120s) since this port isn't upstreamed and won't be
   assigned a real number by that process. */
#define MODEL_NUMBER 500

#define MODEL_NAME   "Anbernic RG35XX Pro"

#ifndef SIMULATOR
#define USB_NONE
#endif

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

/* The panel is physically 640x480, but very few themes target that
   resolution -- render at 320x240 (matches the common QVGA form factor most
   themes are built for) and let the SDL renderer integer-upscale (2x) onto
   the real panel. See window-sdl.c's SDL_RenderSetIntegerScale() call. */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240
#define LCD_DEPTH  16
#define LCD_PIXELFORMAT RGB565

/* define this to indicate your device's keypad */
#define HAVE_BUTTON_DATA
#define HAVE_VOLUME_IN_LIST

/* define this if you have a real-time clock */
#define CONFIG_RTC RTC_HOSTED

/* The number of bytes reserved for loadable codecs */
#define CODEC_SIZE 0x200000

/* The number of bytes reserved for loadable plugins */
#define PLUGIN_BUFFER_SIZE 0x200000

#define AB_REPEAT_ENABLE

/* Battery stuff.  The kernel's fuel gauge gives us a percentage directly, so
   no hand-tuned voltage curve (and hence no percent_to_volt_* tables) are
   needed -- see the BATTERY_CAPACITY_DEFAULT guard in firmware/powermgmt.c */
#define CONFIG_BATTERY_MEASURE PERCENTAGE_MEASURE
#define CONFIG_CHARGING CHARGING_MONITOR
#define HAVE_POWEROFF_WHILE_CHARGING
#define BATTERY_DEV_NAME "axp2202-battery"
#define POWER_DEV_NAME "axp2202-usb"

/* Define this for LCD backlight available */
#define BACKLIGHT_RG35XX_PRO
#define HAVE_BACKLIGHT
#define HAVE_BACKLIGHT_BRIGHTNESS
#define CONFIG_BACKLIGHT_FADING BACKLIGHT_FADING_SW_SETTING

/* Main LCD backlight brightness range and defaults.  This is the Rockbox-side
   scale; the driver rescales it onto whatever max_brightness the panel
   reports. */
#define MIN_BRIGHTNESS_SETTING      0
#define MAX_BRIGHTNESS_SETTING      10
#define DEFAULT_BRIGHTNESS_SETTING  5

#define CONFIG_KEYPAD RG35XX_PRO_PAD

/* Use SDL audio/pcm/video in a SDL app build */
#define HAVE_SDL
#define HAVE_SDL_AUDIO

/* KNULLI exposes the built-in controls as an evdev gamepad, which SDL2
   surfaces as a joystick rather than as keyboard keys */
#ifndef SIMULATOR
#define HAVE_SDL_JOYSTICK
#endif

#define HAVE_SW_TONE_CONTROLS

#define CONFIG_LCD LCD_COWOND2

/* Define this if a programmable hotkey is mapped */
#define HAVE_HOTKEY

#define BOOTDIR "/.rockbox"

/* No special storage */
#define CONFIG_STORAGE STORAGE_HOSTFS
#define HAVE_STORAGE_FLUSH
