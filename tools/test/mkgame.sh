#!/bin/bash
# Makes game/ and prefix/ for the tests from the ones of the main checkout: in a git worktree, so several worktrees
# can run the game at the same time, or with --into in any directory (a slot of the scrap MCP server). The data that
# is only read (the .packed files, Video, tmp, ...) is linked; what the game writes or the tests change (Bin with
# scrapdecomp.ini, Scrap.cfg, commonappdata, the logs and outputs in game/) is a copy, and so is the Wine prefix.
# Nothing of the main checkout is changed.
#
# usage: tools/test/mkgame.sh [main checkout]   (run inside the worktree; default /home/claude/projects/scrapland)
#        tools/test/mkgame.sh --into <dir> [main checkout]   (<dir>/game and <dir>/prefix)
#        tools/test/mkgame.sh --native <game dir> [main checkout]   (a game folder for the native Linux build: no
#                   prefix, and none of the Windows files of Bin)
root=$(cd "$(dirname "$0")/../.." && pwd)
native=
if [ "$1" = --native ]; then
	native=$2
	mkdir -p "$native" && native=$(cd "$native" && pwd) || exit 1
	root=$native
	shift 2
elif [ "$1" = --into ]; then
	mkdir -p "$2" && root=$(cd "$2" && pwd) || exit 1
	shift 2
fi
main=$(cd "${1:-/home/claude/projects/scrapland}" && pwd) || exit 1
if [ "$root" = "$main" ]; then
	echo "mkgame.sh: this is the main checkout, nothing to do" >&2
	exit 1
fi
[ -d "$main/game/Bin" ] && [ -d "$main/prefix" ] || { echo "mkgame.sh: no game/ or prefix/ in $main" >&2; exit 1; }

g=$root/game
[ -n "$native" ] && g=$native
mkdir -p "$g/Bin" || exit 1
for f in "$main"/game/*; do
	n=$(basename "$f")
	case $n in
	Bin | Scrap.cfg | commonappdata | log.html | logAI.html | sd*) ;;
	*) ln -sfn "$f" "$g/$n" ;;
	esac
done
cp -a "$main/game/Scrap.cfg" "$g/"
rm -rf "$g/commonappdata"
cp -a "$main/game/commonappdata" "$g/"
if [ -n "$native" ]; then
	# the scripts and the tests look for it
	ln -sfn "$main/game/Bin/Scrap.exe" "$g/Bin/Scrap.exe"
	# the folder of the configuration and the saves (XDG_DATA_HOME of native.sh) as in the Wine prefix
	rm -rf "$g/xdg-data"
	mkdir -p "$g/xdg-data/MercurySteam"
	cp -a "$main/prefix/drive_c/users/claude/Documents/MercurySteam/Scrapland" "$g/xdg-data/MercurySteam/" 2> /dev/null ||
		mkdir -p "$g/xdg-data/MercurySteam/Scrapland"
	echo "mkgame.sh: native game folder ready in $g"
	exit 0
fi
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
