# Rockbox

My personal fork of [Rockbox](https://www.rockbox.org/).

Includes GitHub actions workflows for producing builds for the targets I use.

Upstream is merged daily.

## Building locally

CI builds inside the `ghcr.io/adriankeenan/rockbox-env` container image
(built from the [`Dockerfile`](Dockerfile)), which already has the
toolchains for this fork's targets baked in. The easiest way to reproduce a
build locally is to use the same image:

```sh
docker run --rm -it -v "$PWD":/rockbox -w /rockbox ghcr.io/adriankeenan/rockbox-env:latest bash
```

Then, inside the container:

```sh
mkdir build && cd build
../tools/configure --target=erosqnative --type=n
make -j$(nproc)
make zip
```

Substitute `--target=rg35xxpro` and `make port-zip` to build the Anbernic
RG35XX Pro target instead. See `.github/workflows/device-build.yml` for the
exact `configure_target`/`zip_target` values CI uses for each target.

If you'd rather not pull the prebuilt image, build it locally with
`docker build -t rockbox-env .` and use that tag instead.

This only covers the container-based flow for this fork's targets. For
building on bare metal with a manually-installed toolchain, or for targets
other than the two above, see the upstream instructions in
[`docs/README`](docs/README).

## New targets

⚠️ New targets have been added using LLMs. While these targets have been tested
extensively on real hardware, the code changes themselves are largely unreviewed.
Good luck! 🙏

| Target | `--target=` | CFW |
|---|---|---|
| [Anbernic RG35XX Pro](#anbernic-rg35xx-pro) | `rg35xxpro` | KNULLI |

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
