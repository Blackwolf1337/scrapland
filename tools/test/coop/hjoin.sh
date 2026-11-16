#!/bin/bash
# a headless player of a world (-join -headless): hjoin.sh <name> <port> [build dir] [extra args...]
# own game folder g-<name> (own config folder: own identity key), log <name>.log, pid <name>.pid; env passes through
. "$(dirname "$(readlink -f "$0")")/env.sh"
name=$1 port=$2 build=${3:-$R/build-linux64}
shift $(( $# < 3 ? $# : 3 ))
g=$S/g-$name
[ -d "$g/Bin" ] || "$R/tools/test/mkgame.sh" --native "$g" > /dev/null 2>&1 || exit 1
# never cp over a running binary: a copy beside it and a rename
cmp -s "$build/ScrapC" "$g/Bin/ScrapC" || { cp -a "$build/ScrapC" "$g/Bin/ScrapC.new" && mv -f "$g/Bin/ScrapC.new" "$g/Bin/ScrapC"; } || exit 1
for l in "$build"/ffmpeg/install/lib/lib*.so*; do [ -e "$g/Bin/$(basename "$l")" ] || cp -a "$l" "$g/Bin/" || exit 1; done
cd "$g/Bin" || exit 1
env -u DISPLAY -u WAYLAND_DISPLAY XDG_DATA_HOME=$g/xdg-data SCRAP_NO_MSGBOX=1 PULSE_SERVER=unix:/nonexistent \
	PIPEWIRE_REMOTE=/nonexistent SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ${HJOIN_GDB:+gdb -q -batch -nx -ex "break CheckFailed" -ex run -ex "bt 30" --args} ./ScrapC -join:127.0.0.1 -port:$port \
	-name:${PLAYER_NAME:-${name//-/_}} -headless "$@" < /dev/null >> "$S/$name.log" 2>&1 &
echo $! > "$S/$name.pid"
echo "headless player $name pid $!"
