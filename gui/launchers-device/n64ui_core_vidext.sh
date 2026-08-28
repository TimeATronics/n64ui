#!/bin/sh
EMU_DIR=/mnt/SDCARD/Emus/N64
MP_DIR="$EMU_DIR/n64ui-test"

$EMU_DIR/cpufreq-OLD.sh

export XDG_CONFIG_HOME="$EMU_DIR"
export XDG_DATA_HOME="$EMU_DIR"
export HOME="$EMU_DIR"

cd "$MP_DIR"
export LD_LIBRARY_PATH="$MP_DIR:/usr/trimui/lib:/usr/lib:$LD_LIBRARY_PATH"

ROM_PATH="$1"
case "$ROM_PATH" in
    *.zip|*.7z)
        TEMP_ROM=$(mktemp)
        /mnt/SDCARD/Apps/PortMaster/PortMaster/7zzs.aarch64 e "$ROM_PATH" -so > "$TEMP_ROM" 2>/dev/null
        ROM_PATH="$TEMP_ROM"
        ;;
esac

# A/B test: use the core's built-in vidext (stock behavior) instead of ours.
export N64UI_CORE_VIDEXT=1
exec ./n64ui "$ROM_PATH" > /tmp/n64ui-launcher.log 2>&1
