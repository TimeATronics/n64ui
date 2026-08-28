#!/bin/sh
# N64UI launch script (Apps/N64UI on the Brick, or as the Emus/N64 entry).
# The emulator payload lives next to the binary in MP_DIR.
MP_DIR=/mnt/SDCARD/Emus/N64/mupen64plus

export XDG_CONFIG_HOME=/mnt/SDCARD/Emus/N64
export XDG_DATA_HOME=/mnt/SDCARD/Emus/N64
export LD_LIBRARY_PATH="$MP_DIR:/usr/trimui/lib:/usr/lib:$LD_LIBRARY_PATH"

cd "$MP_DIR" || exit 1
exec ./n64ui "$@"
