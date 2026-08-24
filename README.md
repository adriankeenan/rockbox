# Rockbox

My personal fork of [Rockbox](https://www.rockbox.org/).

Includes GitHub actions workflows for producing builds for the targets I use.

Upstream is merged daily. If the merge conflicts, the conflicted merge is
committed as-is (conflict markers and all) and pushed to an `upstream-merge`
branch, and the workflow refuses to run again while that branch exists. To
recover: check out `upstream-merge`, fix the conflicts and amend the merge
commit, merge the branch into `master`, delete the branch, then re-run the
workflow.

## Building locally

CI builds inside a container image (built from the [`Dockerfile`](Dockerfile))
which already has the toolchains for this fork's targets baked in. Build the
image locally and use it the same way:

```sh
docker build -t rockbox-env .
docker run --rm -it -v "$PWD":/rockbox -w /rockbox rockbox-env bash
```

Then, inside the container:

```sh
mkdir build && cd build
../tools/configure --target=erosqnative --type=n
make -j$(nproc)
make zip
```

Substitute `--target=rg35xxpro` and `make port-zip` to build the Anbernic
RG35XX Pro target instead. See
[`.github/workflows/device-build.yml`](.github/workflows/device-build.yml)
for the exact `configure_target`/`zip_target` values CI uses for each
target, and
[`.github/workflows/build-image.yml`](.github/workflows/build-image.yml)
for how the container image itself is built and published.

## New targets

⚠️ New targets have been added using LLMs. While these targets have been tested
extensively on real hardware, the code changes themselves are largely unreviewed.
Good luck! 🙏

| Target | `--target=` | CFW |
|---|---|---|
| [Anbernic RG35XX Pro](#anbernic-rg35xx-pro-on-knulli) | `rg35xxpro` | KNULLI |
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
- Audio: SDL2. Volume handled by the system, rockbox output set to 100% and
  volume keys mapped out. Internal speaker and 3.5mm jack work without issues.
- Power: left entirely to the system, use power button as usual.

**Known issues**

None!

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
| Start | Lock keys |
| L1 | Hotkey / WPS shortcut |
| R1 | Quickscreen |
| Vol Up / Vol Down | System-only |
| Power | System-only |
| L2, R2, analog stick click | Unbound |
| FN | No hardware control; keyboard-only |

### Sony PSP

Runs as PSP homebrew, installed into `PSP/GAME` and launched from the XMB's
Game menu. Needs Custom Firmware or a Homebrew Enabler; also runs directly
in PPSSPP.

**Hardware**

- SoC: Sony/NEC "Allegrex" (MIPS, single core)
- Screen: 4.3" 480x272. Rockbox renders at 320x240 (QVGA, for theme
  compatibility) unscaled and centered, with black borders on all sides
  rather than upscaled.
- Colour depth: 16-bit (RGB565)
- Audio: pspaudiolib (`sceAudio`). Volume handled by the system, rockbox
  output set to 100% and volume keys mapped out. Internal speaker and 3.5mm
  jack work without issues.
- Power: battery percentage and charging state read via `scePower`.

**Known issues**

- Plugins are disabled -- they need per-plugin PSP keymap entries first.

**Installation**

Copy the release zip contents onto the memory stick root, giving
`ms0:/PSP/GAME/ROCKBOX/EBOOT.PBP` alongside its `fonts/`, `themes/`,
`codecs/`, etc. Launch "Rockbox" from the XMB's Game menu.

**Keymap**

| Button | Action |
|---|---|
| D-pad / Analog stick | Navigate / seek |
| Cross | Select / Play-Pause |
| Circle | Back / Browse (WPS) |
| Triangle | Menu / context |
| Square | Hotkey / WPS shortcut |
| Start | Stop (hold) |
| Square + Start | Keylock |
| Home | Hold: power off |
| L / R | As D-pad Left/Right (WPS): skip track, hold to seek |
| Volume Up / Volume Down | System-only |
| Select | Unbound |
