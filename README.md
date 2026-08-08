# Rockbox (adriankeenan fork)

This fork tracks upstream [Rockbox](https://www.rockbox.org/) automatically
(`.github/workflows/upstream-merge.yml` merges `upstream/master` in daily) and
adds support for handheld targets that aren't part of the upstream project.

## New targets

The targets below are fork-specific, not upstream. Each is built as a
**Rockbox application port** rather than replacement firmware: Rockbox runs
on top of the device's existing Linux-based custom firmware (CFW) and is
launched like an app, instead of replacing the CFW's bootloader/OS. They're
included in the weekly CI build (`.github/workflows/device-build.yml`) and
buildable locally via `tools/configure --target=<name>`.

| Target | `--target=` | CFW |
|---|---|---|
| [Anbernic RG35XX Pro](#anbernic-rg35xx-pro) | `rg35xxpro` | KNULLI |

### Anbernic RG35XX Pro

Runs as an application port under [KNULLI](https://knulli.org/) (a
Batocera-derived CFW), installed into `roms/ports` and launched from
EmulationStation's Ports collection like any other port. Quitting Rockbox
returns to EmulationStation.

**Hardware**

- SoC: Allwinner H700 (quad-core Cortex-A53, aarch64)
- Screen: 3.5" 640x480 IPS. Rockbox renders internally at 320x240 (the
  common QVGA size most themes target) and integer-upscales 2x onto the
  panel -- see `LCD_WIDTH`/`LCD_HEIGHT` in
  `firmware/export/config/rg35xxpro.h`.
- Colour depth: 16-bit (RGB565)
- Audio: SDL2 audio. Volume is controlled by the system only -- KNULLI's own
  volume daemon drives it from the same physical buttons Rockbox reads, so
  Rockbox's own software volume is pinned to unity (100%) rather than the
  two stacking. See `firmware/drivers/audio/sdl.c`.
- Power: the physical Power button is left entirely to the system
  (suspend/shutdown) and is not read by Rockbox.

**Installation**

Copy the contents of the release zip (`rockbox-rg35xxpro.zip`) onto the
device's SHARE partition at `/userdata/roms/ports/`, so you end up with
`roms/ports/rockbox.sh` and `roms/ports/rockbox/`. Launch "Rockbox" from the
Ports collection in EmulationStation.

**Keymap**

| Button | Action |
|---|---|
| D-pad / Analog stick | Navigate / seek |
| A | Select / Play-Pause |
| B | Back / Stop |
| X | ID3 info screen (WPS) / open WPS from the file browser |
| Y | Delete bookmark (bookmark screen only) |
| Menu | Tap: open main menu. Hold: exit Rockbox, back to EmulationStation |
| Select | Context menu |
| L1 | Hotkey / WPS shortcut |
| R1 | Quickscreen |
| Start | Confirm text entry (on-screen keyboard only) |
| Vol Up / Vol Down | Handled by the system, not Rockbox |
| Power | Handled by the system, not Rockbox |
| L2, R2, analog stick click | Recognised but not currently bound to any action |
| FN | No physical control triggers this on hardware -- keyboard-only, e.g. a debug build with a USB keyboard attached |

See `apps/keymaps/keymap-rg35xxpro.c` for the full action mapping and
`firmware/target/hosted/anbernic/rg35xxpro/button-rg35xxpro.c` for the raw
button/joystick index mapping.
