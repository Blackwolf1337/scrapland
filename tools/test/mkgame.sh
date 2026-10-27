#!/bin/bash
# Makes game/ and prefix/ for the tests in a git worktree from the ones of the main checkout, so several worktrees can
# run tools/test/difftest.sh at the same time. The data that is only read (the .packed files, Video, tmp, ...) is
# linked; what the game writes or the tests change (Bin with dinput8.dll and scrapdecomp.ini, Scrap.cfg,
# commonappdata, the logs and outputs in game/) is a copy, and so is the Wine prefix. Nothing of the main checkout is
# changed.
#
# usage: tools/test/mkgame.sh [main checkout]   (run inside the worktree; default /home/claude/projects/scrapland)
root=$(cd "$(dirname "$0")/../.." && pwd)
main=$(cd "${1:-/home/claude/projects/scrapland}" && pwd) || exit 1
if [ "$root" = "$main" ]; then
	echo "mkgame.sh: this is the main checkout, nothing to do" >&2
	exit 1
fi
[ -d "$main/game/Bin" ] && [ -d "$main/prefix" ] || { echo "mkgame.sh: no game/ or prefix/ in $main" >&2; exit 1; }

mkdir -p "$root/game/Bin" || exit 1
for f in "$main"/game/*; do
	n=$(basename "$f")
	case $n in
	Bin | Scrap.cfg | commonappdata | log.html | logAI.html | sd*) ;;
	*) ln -sfn "$f" "$root/game/$n" ;;
	esac
done
cp -a "$main/game/Scrap.cfg" "$root/game/"
rm -rf "$root/game/commonappdata"
cp -a "$main/game/commonappdata" "$root/game/"
for f in "$main"/game/Bin/*; do
	n=$(basename "$f")
	case $n in
	dinput8.dll | scrapdecomp.*) ;;
	*) rm -f "$root/game/Bin/$n"; cp -a "$f" "$root/game/Bin/" ;;
	esac
done
if [ ! -d "$root/prefix" ]; then
	cp -a "$main/prefix" "$root/prefix" || exit 1
fi
echo "mkgame.sh: game/ and prefix/ ready in $root"
