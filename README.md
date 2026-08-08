# Rockbox (adriankeenan fork)

Tracks upstream [Rockbox](https://www.rockbox.org/) automatically
(`.github/workflows/upstream-merge.yml` merges `upstream/master` daily) and
adds support for handheld targets not in the upstream project.

## New targets

Fork-specific targets, built as **application ports** rather than
replacement firmware -- Rockbox runs on top of the device's existing Linux
CFW and launches like an app instead of replacing the CFW's bootloader/OS.
Included in the weekly CI build (`.github/workflows/device-build.yml`) and
buildable locally via `tools/configure --target=<name>`.

| Target | `--target=` | CFW |
|---|---|---|
| [Anbernic RG35XX Pro](#anbernic-rg35xx-pro) | `rg35xxpro` | KNULLI |

### Anbernic RG35XX Pro

Runs under [KNULLI](https://knulli.org/), installed into `roms/ports` and
launched from EmulationStation's Ports collection. Quitting returns to
EmulationStation.

**Hardware**

- SoC: Allwinner H700 (quad-core Cortex-A53, aarch64)
- Screen: 3.5" 640x480 IPS. Rockbox renders at 320x240 (QVGA, for theme
  compatibility) and integer-upscales 2x onto the panel.
- Colour depth: 16-bit (RGB565)
- Audio: SDL2. Volume is system-only -- KNULLI's volume daemon and Rockbox
  read the same buttons, so Rockbox's own volume is pinned to unity to
  avoid double-adjusting.
- Power: left entirely to the system (suspend/shutdown); not read by Rockbox.

**Installation**

Copy the release zip (`rockbox-rg35xxpro.zip`) contents onto the device's
SHARE partition at `/userdata/roms/ports/`, giving `roms/ports/rockbox.sh`
and `roms/ports/rockbox/`. Launch "Rockbox" from EmulationStation's Ports.

**Keymap**

| Button | Action |
|---|---|
| D-pad / Analog stick | Navigate / seek |
| A | Select / Play-Pause |
| B | Back / Stop |
| X | ID3 info screen (WPS) / open WPS from file browser |
| Y | Delete bookmark (bookmark screen only) |
| Menu | Tap: main menu. Hold: exit to EmulationStation |
| Select | Context menu |
| L1 | Hotkey / WPS shortcut |
| R1 | Quickscreen |
| Start | Confirm text entry (on-screen keyboard only) |
| Vol Up / Vol Down | System-only |
| Power | System-only |
| L2, R2, analog stick click | Unbound |
| FN | No hardware control; keyboard-only |

Full mapping: `apps/keymaps/keymap-rg35xxpro.c` (actions) and
`firmware/target/hosted/anbernic/rg35xxpro/button-rg35xxpro.c` (raw indices).
