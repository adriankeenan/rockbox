# Rockbox

My personal fork of [Rockbox](https://www.rockbox.org/).

Includes GitHub actions workflows for producing builds for the targets I use.

Upstream is merged daily.

## New targets

⚠️ New targets have been added using LLMs. While these targets have been tested
extensively on real hardware, the code changes themselves are largely unreviewed.
Good luck! 🙏

| Target | `--target=` | CFW |
|---|---|---|
| [Anbernic RG35XX Pro](#anbernic-rg35xx-pro) | `rg35xxpro` | KNULLI |
| [Sony PSP](#sony-psp) | `psp` | Homebrew (CFW/HEN) |

### Anbernic RG35XX Pro on Knulli

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

### Sony PSP

Runs as PSP homebrew (PSPSDK/newlib, no dynamic ELF loader available, so
codecs are linked at a fixed address and loaded with a raw read() instead
of dlopen -- same scheme as the native, non-`APPLICATION` targets). Ships
as `EBOOT.PBP`, launched from the XMB via Custom Firmware or a Homebrew
Enabler, or opened directly in PPSSPP.

**Hardware**

- SoC: Sony/NEC "Allegrex" (MIPS, single core)
- Screen: 4.3" 480x272. Rockbox renders 320x240 (QVGA, for theme
  compatibility) unscaled and centered, with black borders on all sides
  rather than upscaled.
- Colour depth: 16-bit (RGB565)
- Audio: pspaudiolib (`sceAudio`). Volume is real: Rockbox's own volume
  setting drives the audio driver's per-channel hardware gain
  (`sceAudioChangeChannelVolume`) directly, rather than software mixing.
- Power: battery percentage and charging state via `scePower`.

**Installation**

Copy the release zip contents onto the memory stick root, giving
`ms0:/PSP/GAME/ROCKBOX/EBOOT.PBP` alongside its `fonts/`, `themes/`,
`codecs/`, etc. Launch "Rockbox" from the XMB's Game menu.

**Keymap**

| Button | Action |
|---|---|
| D-pad / Analog stick | Navigate / seek / volume (WPS) |
| Cross | Select / Play-Pause |
| Circle | Back / Browse (WPS) |
| Triangle | Menu / context |
| Square | Hotkey / WPS shortcut |
| Start | Stop (hold) |
| Square + Start | Keylock |
| Home | Hold: power off |
| L, R, Select | Unbound |

Full mapping: `apps/keymaps/keymap-psp.c`.

**Known limitations**

- Plugins are disabled -- they need per-plugin PSP keymap entries first.
- Backlight brightness is fixed; `sceDisplaySetBrightness` needs
  kernel-mode access this SDK doesn't expose.
