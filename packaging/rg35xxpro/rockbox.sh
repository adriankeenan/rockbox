#!/bin/sh
#
# Rockbox launcher for the Anbernic RG35XX Pro running the KNULLI CFW.
#
# Install this script and the accompanying "rockbox" directory into
# /userdata/roms/ports/ (the SHARE partition), then launch Rockbox from the
# Ports collection in EmulationStation.  Quitting Rockbox returns here, and
# EmulationStation resumes.

RBDIR="$(dirname "$0")/rockbox"

cd "$RBDIR" || exit 1

# Rockbox uses SDL2 for both video and audio.  The video driver is left to
# SDL's own autodetection because it differs between KNULLI releases -- if the
# screen stays blank, try forcing one by exporting SDL_VIDEODRIVER before
# launching, e.g. SDL_VIDEODRIVER=kmsdrm or SDL_VIDEODRIVER=fbdev.
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"

exec ./rockbox
