#!/bin/bash
# Runs the native Linux ScrapC (build-linux64/ScrapC) hidden and muted through headless.sh, in a game folder made by
# tools/test/mkgame.sh --native (default build-linux64/game, the Bin there gets the binary and its FFmpeg libraries
# fresh from the build at every run). The configuration and the saves go to <game>/xdg-data. Sound: the null device
# of the mixer (SCRAP_SND_NULL=1) and PulseAudio and PipeWire pointed at sockets that do not exist. The working
# directory is game/Bin, as for the Windows exe.
#
# usage: tools/test/native.sh [arguments of the game]
#        NATIVE_LOG   file for the output of the program, default nowhere
#        HEADLESS_TIMEOUT, HEADLESS_SIZE as in headless.sh; SCRAP_VK_DEVICE (default the AMD 610M, 1002:164e)
#        SCRAP_VK_VALIDATION=1   the Vulkan validation layer for the program (messages in NATIVE_LOG)
#        SCRAP_GAME_DIR   another game folder, SCRAP_BUILD another build folder (default build-linux64)
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${SCRAP_BUILD:-$root/build-linux64}
game=${SCRAP_GAME_DIR:-$build/game}
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
cp -a "$build/ScrapC" "$game/Bin/" || exit 1
cp -a "$build"/ffmpeg/install/lib/lib*.so* "$game/Bin/" || exit 1
export PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=/nonexistent SCRAP_SND_NULL=1
# a message box would wait for a button nobody presses: its text goes to scrapdecomp.log
export SCRAP_NO_MSGBOX=1
# the configuration and the saves in the game folder (SDL's user data folder), never in the home of the user
export XDG_DATA_HOME=$game/xdg-data
export SCRAP_VK_DEVICE=${SCRAP_VK_DEVICE:-1002:164e}
cd "$game/Bin" || exit 1
layers=()
[ "${SCRAP_VK_VALIDATION:-0}" = 1 ] && layers=(VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation)
HEADLESS_LOG=${NATIVE_LOG:-/dev/null} exec "$root/tools/test/headless.sh" "${layers[@]}" ./ScrapC "$@"
