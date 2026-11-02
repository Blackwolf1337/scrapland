#!/bin/bash
# Runs a Windows program of the game in the project Wine prefix (prefix/, never ~/.wine), hidden and muted through
# headless.sh. Sound: the prefix has HKCU\Software\Wine\Drivers Audio="" (no audio driver at all, set by
# tools/test/prefix.sh) and PulseAudio/PipeWire point to sockets that do not exist. Switching off the driver DLLs
# through WINEDLLOVERRIDES alone was not enough, the game was audible.
# The working directory is game/Bin, like the GOG launcher (playTasks path "Bin/Scrap.exe").
#
# usage: tools/test/wine.sh <exe in game/Bin> [arguments]
#        WINE_LOG   file for the output of Wine and the program, default nowhere
#        HEADLESS_TIMEOUT, HEADLESS_SIZE as in headless.sh; SCRAP_VK_DEVICE (default the AMD 610M, 1002:164e)
#        SCRAP_VK_VALIDATION=1   the Vulkan validation layer for the program (messages in WINE_LOG)
#        SCRAP_GAME_DIR, SCRAP_PREFIX   another game/ and prefix/ (a slot of mkgame.sh --into), default those of the
#                   checkout
root=$(cd "$(dirname "$0")/../.." && pwd)
game=${SCRAP_GAME_DIR:-$root/game}
export WINEPREFIX=${SCRAP_PREFIX:-$root/prefix} WINEARCH=win64 WINEDEBUG=${WINEDEBUG:--all}
export PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=/nonexistent
# the Vulkan renderer on the AMD 610M (the RTX is shared with other guests)
export SCRAP_VK_DEVICE=${SCRAP_VK_DEVICE:-1002:164e}
overrides="winepulse.drv,winealsa.drv=d;mscoree,mshtml=${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"
cd "$game/Bin" || exit 1
# the validation layer only for the program, not for gamescope (whose own messages would mix in)
layers=()
[ "${SCRAP_VK_VALIDATION:-0}" = 1 ] && layers=(VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation)
# shellcheck disable=SC2016 # expanded by the shell inside gamescope
HEADLESS_LOG=${WINE_LOG:-/dev/null} "$root/tools/test/headless.sh" WINEDLLOVERRIDES="$overrides" "${layers[@]}" \
	sh -c 'wine "$@"; rc=$?; wineserver -k; exit $rc' wine "$@"
rc=$?
# a timeout kills gamescope but not the Wine services of the prefix, and a wineserver that is already gone leaves
# winedevice.exe behind, so every process that still runs with this prefix gets killed too
wineserver -k9 2> /dev/null
for env in /proc/[0-9]*/environ; do
	grep -qzx "WINEPREFIX=$WINEPREFIX" "$env" 2> /dev/null && kill -9 "$(basename "$(dirname "$env")")" 2> /dev/null
done
exit $rc
