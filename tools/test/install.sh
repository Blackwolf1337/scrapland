#!/bin/bash
# Copies the built ScrapC.exe (build-win64) and the DLLs of FFmpeg and SDL3 into game/Bin. Refuses while the game of
# this checkout runs (one started with its prefix; worktrees made by mkgame.sh have their own); copies to .new and
# renames, so a reader never sees a half written file.
root=$(cd "$(dirname "$0")/../.." && pwd)
for pid in $(pgrep -x 'Scrap\.exe|ScrapC\.exe|Dedicated\.exe'); do
	if grep -qzx "WINEPREFIX=$root/prefix" "/proc/$pid/environ" 2> /dev/null; then
		echo "install.sh: the game is running, not copying" >&2
		exit 1
	fi
done
for f in ScrapC.exe; do
	src=$root/build-win64/$f
	dst=$root/game/Bin/$f
	[ -f "$src" ] || { echo "install.sh: $src missing, build first" >&2; exit 1; }
	cp "$src" "$dst.new" && mv "$dst.new" "$dst" && echo "installed $dst" || exit 1
done
# the DLLs of FFmpeg and SDL3 (tools/ffmpeg.sh, tools/sdl3.sh) when they changed
for src in "$root"/build-win64/ffmpeg/install/bin/*.dll "$root"/build-win64/sdl3/install/bin/SDL3.dll; do
	dst=$root/game/Bin/$(basename "$src")
	cmp -s "$src" "$dst" || { cp "$src" "$dst.new" && mv "$dst.new" "$dst" && echo "installed $dst"; } || exit 1
done
