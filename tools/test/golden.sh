#!/bin/bash
# The golden test of the 64-bit Linux build: about 10000 checks of the script functions of the game and of the
# interpreter run in the main menu, hidden and muted, and the output is compared with the stored one,
# tools/test/golden/sdtest.out. The game folder is <build>/golden (made by mkgame.sh --native).
#
# GOLDEN_MODE=channel (the default): the game runs with the test channel (src/game/Global/TestChannel.c) and no
#   -PythonExecute; golden/run.py sends the API checks of golden/sdtest.cmd as channel commands, then the interpreter
#   checks golden/sdpy.py through the channel's py command (STAGE 7: they go with the interpreter).
# GOLDEN_MODE=python: the old way, golden/sdtest.py through -PythonExecute (the API checks and sdpy.py in Python).
# GOLDEN_MODE=record: sdtest.py with the recorder golden/sdrec.py through -PythonExecute, then golden/mkcmd.py makes
#   golden/sdtest.cmd again from it (review the difference before committing it).
# A build without the interpreter (cmake -DSCRAP_NO_PYTHON=ON) runs only the channel mode and only the API checks:
#   the interpreter checks of sdpy.py are skipped (they test what that build does not have) and the output is
#   compared with the API part of the stored one.
#
# usage: tools/test/golden.sh            (builds nothing: run cmake first)
#        GOLDEN_UPDATE=1                 stores the output as the new expected one (after reviewing the difference)
#        SCRAP_BUILD                     the build folder, default build-linux64
#        GOLDEN_TIMEOUT                  seconds, default 240
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(realpath -m "${SCRAP_BUILD:-$root/build-linux64}")
game=$build/golden
dir=$root/tools/test/golden
mode=${GOLDEN_MODE:-channel}
timeout=${GOLDEN_TIMEOUT:-240}
nopy=0
grep -qx 'SCRAP_NO_PYTHON:BOOL=ON' "$build/CMakeCache.txt" 2> /dev/null && nopy=1
if [ $nopy = 1 ] && [ "$mode" != channel ]; then
	echo "golden: $build has no Python interpreter, only GOLDEN_MODE=channel runs there" >&2
	exit 1
fi
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
cp "$dir"/*.py "$game/" || exit 1
rm -f "$game/sdtest.out" "$game/sdtest.expected" "$game/sdrec.out" "$game"/test_*.txt
out=$game/sdtest.out
expected=$dir/sdtest.out

# kills the game of this folder (a clean exit would save the config)
endgame() {
	for p in /proc/[0-9]*; do
		[ "$(readlink "$p/exe" 2> /dev/null)" = "$game/Bin/ScrapC" ] && kill -9 "${p#/proc/}" 2> /dev/null
	done
}

if [ "$mode" = channel ]; then
	python3 "$dir/run.py" prepare "$game" || exit 1
	SCRAP_TEST_CHANNEL=1 SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=$timeout NATIVE_LOG=$build/golden.log \
		"$root/tools/test/native.sh" -fullscreen:0 &
	pid=$!
	GOLDEN_NOPY=$nopy python3 "$dir/run.py" collect "$game" "$timeout"
	endgame
	if [ $nopy = 1 ]; then
		expected=$game/sdtest.expected
		echo "golden: no Python interpreter in $build, the $(($(grep -c ' = ' "$dir/sdtest.out") - \
			$(grep -c ' = ' "$expected"))) interpreter checks of sdpy.py are skipped"
	fi
else
	script=sdtest.py
	if [ "$mode" = record ]; then
		# sdtest.py with the recorder after the imports and without the interpreter checks
		python3 -I - "$dir/sdtest.py" "$game/sdtrec.py" <<-'EOF'
			import re, sys
			src = open(sys.argv[1]).read().replace("OUT = 'sdtest.out'", "OUT = 'sdrec.out'")
			src = re.sub(r"(import Scrap, SWeap.*\n)", r"\1execfile('sdrec.py')\n", src, count=1)
			open(sys.argv[2], 'w').write(src.replace("execfile('sdpy.py')\n", ''))
		EOF
		script=sdtrec.py
		out=$game/sdrec.out
	fi
	arg="-PythonExecute:execfile($(python3 -I -c "import sys; print('+'.join('chr(%d)' % ord(c) for c in sys.argv[1]))" "$script"))"
	SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=$timeout NATIVE_LOG=$build/golden.log \
		"$root/tools/test/native.sh" -fullscreen:0 "$arg" &
	pid=$!
	# the script writes "done" as its last line; then the game is killed
	while kill -0 "$pid" 2> /dev/null; do
		if tail -n 1 "$out" 2> /dev/null | grep -qx 'done'; then
			endgame
			break
		fi
		sleep 1
	done
fi
wait "$pid" 2> /dev/null
rm -f "$game"/Save/SDTest.sav	# the save game round trip of sdtest.py
if ! tail -n 1 "$out" 2> /dev/null | grep -qx 'done'; then
	echo "golden: output incomplete, see $out and $build/golden.log" >&2
	exit 1
fi
if [ "$mode" = record ]; then
	python3 "$dir/mkcmd.py" "$out" > "$dir/sdtest.cmd" || exit 1
	echo "golden: $(grep -c '^check ' "$dir/sdtest.cmd") checks in $dir/sdtest.cmd"
	exit 0
fi
if [ "${GOLDEN_UPDATE:-0}" = 1 ]; then
	[ $nopy = 1 ] && { echo "golden: GOLDEN_UPDATE needs a build with the interpreter" >&2; exit 1; }
	cp "$out" "$dir/sdtest.out"
	echo "golden: $(grep -c ' = ' "$dir/sdtest.out") checks stored"
	exit 0
fi
if diff -u "$expected" "$out" > "$build/golden.diff"; then
	echo "golden: $(grep -c ' = ' "$out") checks match ($mode$([ $nopy = 1 ] && echo ', no Python'))"
	exit 0
fi
echo "golden: outputs differ, see $build/golden.diff" >&2
exit 1
