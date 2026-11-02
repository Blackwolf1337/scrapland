#!/bin/bash
# Runs the native Linux ScrapC (build-linux/ScrapC) hidden and muted through headless.sh, in a game folder made by
# tools/test/mkgame.sh --native (default build-linux/game, the Bin there gets the binary and its FFmpeg libraries
# fresh from build-linux at every run). Sound: the null device of the mixer (SCRAP_SND_NULL=1) and PulseAudio and
# PipeWire pointed at sockets that do not exist. The working directory is game/Bin, as for the Windows exe.
#
# usage: tools/test/native.sh [arguments of the game]
#        NATIVE_LOG   file for the output of the program, default nowhere
#        HEADLESS_TIMEOUT, HEADLESS_SIZE as in headless.sh; SCRAP_VK_DEVICE (default the AMD 610M, 1002:164e)
#        SCRAP_VK_VALIDATION=1   the Vulkan validation layer for the program (messages in NATIVE_LOG)
#        SCRAP_GAME_DIR   another game folder, SCRAP_BUILD another build folder (default build-linux)
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${SCRAP_BUILD:-$root/build-linux}
game=${SCRAP_GAME_DIR:-$build/game}
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
cp -a "$build/ScrapC" "$game/Bin/" || exit 1
cp -a "$build"/ffmpeg/install/lib/lib*.so* "$game/Bin/" || exit 1
export PULSE_SERVER=unix:/nonexistent PIPEWIRE_REMOTE=/nonexistent SCRAP_SND_NULL=1
export SCRAP_VK_DEVICE=${SCRAP_VK_DEVICE:-1002:164e}
cd "$game/Bin" || exit 1
layers=()
[ "${SCRAP_VK_VALIDATION:-0}" = 1 ] && layers=(VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation)
HEADLESS_LOG=${NATIVE_LOG:-/dev/null} exec "$root/tools/test/headless.sh" "${layers[@]}" ./ScrapC "$@"
