#!/bin/bash
# a rendered player of a world in gamescope: join.sh <name> <port> [extra game args...]; game folder g-<name>, log <name>.log
# env: WATCH_SECS (timeout, default 400), WATCH_DEV (Vulkan device, default by free RTX memory), SCRAP_BUILD; others pass
. "$(dirname "$(readlink -f "$0")")/env.sh"
name=$1 port=$2
shift 2
g=$S/g-$name
[ -d "$g/Bin" ] || "$R/tools/test/mkgame.sh" --native "$g" > /dev/null 2>&1 || exit 1
if [ -z "$WATCH_DEV" ]; then
	free=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits 2>/dev/null | head -1)
	if [ "${free:-0}" -ge 3000 ]; then WATCH_DEV=10de:27b0; else WATCH_DEV=1002:164e; fi
fi
mkdir -p "$S/tmp"
TMPDIR=$S/tmp SCRAP_GAME_DIR=$g SCRAP_VK_DEVICE=$WATCH_DEV HEADLESS_TIMEOUT=${WATCH_SECS:-400} NATIVE_LOG=$S/$name.log \
	SCRAP_BUILD=${SCRAP_BUILD:-$R/build-linux64} SCRAP_TEST_CHANNEL=${WATCH_CHANNEL:-1} XDG_DATA_HOME=$g/xdg-data \
	setsid "$R/tools/test/native.sh" -fullscreen:0 -join:127.0.0.1 -port:$port -name:${PLAYER_NAME:-${name//-/_}} "$@" > /dev/null 2>&1 &
echo $! > "$S/$name.pid"
echo "player $name pid $! device $WATCH_DEV"
