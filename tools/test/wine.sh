#!/bin/bash
# Runs a Windows program of the game in the project Wine prefix (prefix/, never ~/.wine), hidden and muted through
# headless.sh. Sound: the prefix has HKCU\Software\Wine\Drivers Audio="" (no audio driver at all, set by
# tools/test/prefix.sh) and PulseAudio/PipeWire point to sockets that do not exist. Switching off the driver DLLs
# through WINEDLLOVERRIDES alone was not enough, the game was audible.
# The working directory is game/Bin, like the GOG launcher (playTasks path "Bin/Scrap.exe").
#
# usage: tools/test/wine.sh <exe in game/Bin> [arguments]
#        WINE_LOG   file for the output of Wine and the program, default nowhere
#        HEADLESS_TIMEOUT, HEADLESS_SIZE as in headless.sh
root=$(cd "$(dirname "$0")/../.." && pwd)
export WINEPREFIX=$root/prefix WINEARCH=win64 WINEDEBUG=${WINEDEBUG:--all}
export PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=/nonexistent
overrides="winepulse.drv,winealsa.drv=d;mscoree,mshtml=${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"
cd "$root/game/Bin" || exit 1
# shellcheck disable=SC2016 # expanded by the shell inside gamescope
HEADLESS_LOG=${WINE_LOG:-/dev/null} "$root/tools/test/headless.sh" WINEDLLOVERRIDES="$overrides" \
	sh -c 'wine "$@"; rc=$?; wineserver -k; exit $rc' wine "$@"
rc=$?
# a timeout kills gamescope but not the Wine services of the prefix, and a wineserver that is already gone leaves
# winedevice.exe behind, so every process that still runs with this prefix gets killed too
wineserver -k9 2> /dev/null
for env in /proc/[0-9]*/environ; do
	grep -qzx "WINEPREFIX=$WINEPREFIX" "$env" 2> /dev/null && kill -9 "$(basename "$(dirname "$env")")" 2> /dev/null
done
exit $rc
