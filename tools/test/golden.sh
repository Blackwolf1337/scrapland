#!/bin/bash
# The golden test of the 64-bit Linux build: the 1481 checks of the script functions of the game run in the main
# menu, hidden and muted, and the output is compared with the stored one, tools/test/golden/sdtest.out. The game folder
# is <build>/golden (made by mkgame.sh --native). The game runs with the test channel (src/game/Global/TestChannel.c);
# golden/run.py sends the checks of golden/sdtest.cmd as channel commands. sdtest.cmd is kept by hand. (The 8480
# checks of the interpreter itself and the python and record modes went with the interpreter, tag interp-final.)
#
# usage: tools/test/golden.sh            (builds nothing: run cmake first)
#        GOLDEN_UPDATE=1                 stores the output as the new expected one (after reviewing the difference)
#        SCRAP_BUILD                     the build folder, default build-linux64
#        GOLDEN_TIMEOUT                  seconds, default 240
#        SCRAP_TEST_DETERMINISTIC=0      without the deterministic test mode of the game (include/scrap/testdet.h),
#                                        which is on by default: fixed random seed and clock
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(realpath -m "${SCRAP_BUILD:-$root/build-linux64}")
game=$build/golden
dir=$root/tools/test/golden
timeout=${GOLDEN_TIMEOUT:-240}
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
rm -f "$game/sdtest.out" "$game"/test_*.txt
out=$game/sdtest.out
expected=$dir/sdtest.out

# kills the game of this folder (a clean exit would save the config)
endgame() {
	for p in /proc/[0-9]*; do
		[ "$(readlink "$p/exe" 2> /dev/null)" = "$game/Bin/ScrapC" ] && kill -9 "${p#/proc/}" 2> /dev/null
	done
}

python3 "$dir/run.py" prepare "$game" || exit 1
SCRAP_TEST_DETERMINISTIC=${SCRAP_TEST_DETERMINISTIC:-1} SCRAP_TEST_CHANNEL=1 SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=$timeout NATIVE_LOG=$build/golden.log \
	"$root/tools/test/native.sh" -fullscreen:0 &
pid=$!
python3 "$dir/run.py" collect "$game" "$timeout"
endgame
wait "$pid" 2> /dev/null
rm -f "$game"/Save/SDTest.sav	# the save game round trip of the checks
if ! tail -n 1 "$out" 2> /dev/null | grep -qx 'done'; then
	echo "golden: output incomplete, see $out and $build/golden.log" >&2
	exit 1
fi
if [ "${GOLDEN_UPDATE:-0}" = 1 ]; then
	cp "$out" "$expected"
	echo "golden: $(grep -c ' = ' "$expected") checks stored"
	exit 0
fi
if diff -u "$expected" "$out" > "$build/golden.diff"; then
	echo "golden: $(grep -c ' = ' "$out") checks match"
	exit 0
fi
echo "golden: outputs differ, see $build/golden.diff" >&2
exit 1
