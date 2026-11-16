#!/bin/bash
# a world: ScrapCoord with its instances in the game folder g-world: world.sh <tag> <port> [build dir] [coordinator args...]
# stdin fifo <S>/<tag>.in (echo status > it), output <S>/<tag>.log, pid <S>/<tag>.pid, data <S>/<tag>-data (players/,
# logs/<Level>.log of the instances); WORLD_KEEP_DATA=1 keeps the data of an earlier run (a restart)
. "$(dirname "$(readlink -f "$0")")/env.sh"
tag=$1 port=$2 build=${3:-$R/build-linux64}
shift $(( $# < 3 ? $# : 3 ))
g=$S/g-world-$tag
[ -d "$g/Bin" ] || "$R/tools/test/mkgame.sh" --native "$g" > /dev/null 2>&1 || exit 1
# never cp over a running binary: a copy beside it and a rename
cmp -s "$build/ScrapC" "$g/Bin/ScrapC" || { cp -a "$build/ScrapC" "$g/Bin/ScrapC.new" && mv -f "$g/Bin/ScrapC.new" "$g/Bin/ScrapC"; } || exit 1
for l in "$build"/ffmpeg/install/lib/lib*.so*; do [ -e "$g/Bin/$(basename "$l")" ] || cp -a "$l" "$g/Bin/" || exit 1; done
rm -f "$S/$tag.in" "$g"/instance_*.quit
[ -n "$WORLD_KEEP_DATA" ] || rm -rf "$S/$tag-data"
mkfifo "$S/$tag.in"
env -u DISPLAY -u WAYLAND_DISPLAY XDG_DATA_HOME=$g/xdg-data SCRAP_NO_MSGBOX=1 PULSE_SERVER=unix:/nonexistent \
	PIPEWIRE_REMOTE=/nonexistent SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy ${WORLD_ENV} \
	${WORLD_COORD:-$build/src/coord/ScrapCoord} -port:$port -bind:127.0.0.1 -name:coop-$tag -data:$S/$tag-data -gamedir:$g \
	-exe:${WORLD_EXE:-$g/Bin/ScrapC} "$@" 0<> "$S/$tag.in" >> "$S/$tag.log" 2>&1 &
echo $! > "$S/$tag.pid"
echo "world $tag pid $! port $port game $g"
