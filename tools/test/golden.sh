#!/bin/bash
# The golden test of the 64-bit Linux build: tools/test/golden/sdtest.py (the script functions of the game, about 10000
# checks, and the interpreter in sdpy.py) runs in the main menu, hidden and muted, and its output is compared with the
# stored one, tools/test/golden/sdtest.out. The game folder is <build>/golden (made by mkgame.sh --native).
#
# usage: tools/test/golden.sh            (builds nothing: run cmake first)
#        GOLDEN_UPDATE=1                 stores the output as the new expected one (after reviewing the difference)
#        SCRAP_BUILD                     the build folder, default build-linux64
#        GOLDEN_TIMEOUT                  seconds, default 240
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${SCRAP_BUILD:-$root/build-linux64}
game=$build/golden
dir=$root/tools/test/golden
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
cp "$dir"/*.py "$game/" || exit 1
rm -f "$game/sdtest.out"
arg="-PythonExecute:execfile($(python3 -I -c "print('+'.join('chr(%d)' % ord(c) for c in 'sdtest.py'))"))"
SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=${GOLDEN_TIMEOUT:-240} NATIVE_LOG=$build/golden.log \
	"$root/tools/test/native.sh" -fullscreen:0 "$arg" &
pid=$!
# the script writes "done" as its last line; then the game is killed (a clean exit would save the config)
while kill -0 "$pid" 2> /dev/null; do
	if tail -n 1 "$game/sdtest.out" 2> /dev/null | grep -qx 'done'; then
		for p in /proc/[0-9]*; do
			[ "$(readlink "$p/exe" 2> /dev/null)" = "$game/Bin/ScrapC" ] && kill -9 "${p#/proc/}" 2> /dev/null
		done
		break
	fi
	sleep 1
done
wait "$pid" 2> /dev/null
rm -f "$game"/Save/SDTest.sav
if ! tail -n 1 "$game/sdtest.out" 2> /dev/null | grep -qx 'done'; then
	echo "golden: output incomplete, see $game/sdtest.out and $build/golden.log" >&2
	exit 1
fi
if [ "${GOLDEN_UPDATE:-0}" = 1 ]; then
	cp "$game/sdtest.out" "$dir/sdtest.out"
	echo "golden: $(grep -c ' = ' "$dir/sdtest.out") checks stored"
	exit 0
fi
if diff -u "$dir/sdtest.out" "$game/sdtest.out" > "$build/golden.diff"; then
	echo "golden: $(grep -c ' = ' "$game/sdtest.out") checks match"
	exit 0
fi
echo "golden: outputs differ, see $build/golden.diff" >&2
exit 1
