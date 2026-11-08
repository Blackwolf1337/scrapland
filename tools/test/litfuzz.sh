#!/bin/bash
# The check of the native values of the script ports (src/game/Script/ScriptNative.c) against Python 1.5.2: the game
# runs in the main menu, hidden and muted, with the test channel. The game folder is <build>/litfuzz (made by
# mkgame.sh --native). Prints one line per run plus the first differences; the exit code is 0 when there are none
# (and, in an ASan build, no AddressSanitizer report in <build>/litfuzz.log).
#
# The default build has no interpreter (SCRAP_NO_PYTHON, read from <build>/CMakeCache.txt): it checks the stored
# texts, tools/test/litfuzz.expected.xz (tools/test/litstore.py), through the channel's native command: the native
# literal reader must take the texts 1.5.2 took and give their stored repr, str and check() texts, and refuse the
# others. The reference build with the interpreter (cmake -DSCRAP_NO_PYTHON=OFF, e.g. SCRAP_BUILD=build-py64) runs
# tools/test/litfuzz.py in the game through the channel's py command and compares the native literal reader, repr,
# str and check() texts with Script_Literal, eval, repr and str of the linked 1.5.2 (random texts per seed, once with
# the atoms of the nopy-4 fuzz and once with more), and the repr cases of every type copied into native values; with
# LITFUZZ_UPDATE=1 and no differences it writes the texts of the run as the new store.
#
# usage: tools/test/litfuzz.sh [texts per seed, default 3200] [seeds ..., default 1 2 3 4 5]   (reference build; the
#                         stored texts are those of the defaults)
#        SCRAP_BUILD      the build folder, default build-linux64 (build-linux64-asan for ASan)
#        LITFUZZ_STORE=1  the stored texts in the reference build too
#        LITFUZZ_UPDATE=1 the reference build stores the texts of its run (after reviewing the difference)
#        LITFUZZ_TIMEOUT  seconds for the whole run, default 1200
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "${SCRAP_BUILD:-$root/build-linux64}" && pwd) || exit 1
game=$build/litfuzz
timeout=${LITFUZZ_TIMEOUT:-1200}
count=${1:-3200}
shift
seeds=("$@")
[ ${#seeds[@]} -gt 0 ] || seeds=(1 2 3 4 5)
store=$root/tools/test/litfuzz.expected.xz
nopy=0
grep -qx 'SCRAP_NO_PYTHON:BOOL=ON' "$build/CMakeCache.txt" 2> /dev/null && nopy=1
if [ $nopy = 1 ] && [ "${LITFUZZ_UPDATE:-0}" = 1 ]; then
	echo "litfuzz: LITFUZZ_UPDATE needs a build with the interpreter (-DSCRAP_NO_PYTHON=OFF)" >&2
	exit 1
fi
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
cp "$root/tools/test/litfuzz.py" "$root/tools/test/golden/sdlib.py" "$game/" || exit 1
rm -f "$game"/test_*.txt

# kills the game of this folder (a clean exit would save the config)
endgame() {
	for p in /proc/[0-9]*; do
		[ "$(readlink "$p/exe" 2> /dev/null)" = "$game/Bin/ScrapC" ] && kill -9 "${p#/proc/}" 2> /dev/null
	done
}

SCRAP_TEST_CHANNEL=1 SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=$timeout NATIVE_LOG=$build/litfuzz.log \
	"$root/tools/test/native.sh" -fullscreen:0 2> /dev/null &
pid=$!
channel() {
	CHANNEL_TIMEOUT=$timeout python3 "$root/tools/test/channel.py" "$game" "$@"
}
fail=0
if [ $nopy = 1 ] || [ "${LITFUZZ_STORE:-0}" = 1 ]; then
	[ $nopy = 1 ] && echo "litfuzz: no Python interpreter in $build, the stored texts of the reference build run"
	if ! python3 "$root/tools/test/channel.py" "$game" --wait 300 > /dev/null; then
		echo "litfuzz: the game did not answer" >&2
		fail=1
	else
		python3 "$root/tools/test/litstore.py" check "$game" "$store" || fail=1
	fi
elif ! out=$(python3 "$root/tools/test/channel.py" "$game" --wait 300 > /dev/null && channel 'native py') ||
	[ "$out" != ok ]; then
	echo "litfuzz: the game did not answer or has no native command: $out" >&2
	fail=1
else
	update=${LITFUZZ_UPDATE:-0}
	results=$(channel py "execfile('litfuzz.py')" "$([ "$update" = 1 ] && echo 'record()')" 'result = reprs()')
	for seed in "${seeds[@]}"; do
		for wide in 0 1; do
			results+=$'\n'$(channel py "result = run($count, $seed, $wide)")
		done
	done
	# the answers are reprs of the result texts
	python3 -I -c 'import ast, sys
for line in sys.stdin.read().split("\n"):
	try:
		print(ast.literal_eval(line))
	except Exception:
		print(line)' <<< "$results"
	[ "$(grep -Ec "^'(repr cases|seed) [^']*, bad 0'$" <<< "$results")" = $((1 + 2 * ${#seeds[@]})) ] || fail=1
	if [ "$update" = 1 ] && [ $fail = 0 ]; then
		saved=$(channel py "result = save('litfuzz-store.txt')")
		python3 "$root/tools/test/litstore.py" save "$game/litfuzz-store.txt" "$store" \
			"litfuzz.py run $count, seeds ${seeds[*]}, wide 0 and 1" || fail=1
		[ $fail = 0 ] || echo "litfuzz: storing failed: $saved" >&2
	fi
fi
endgame
wait "$pid" 2> /dev/null
if grep -q AddressSanitizer "$build/litfuzz.log" 2> /dev/null; then
	echo "litfuzz: AddressSanitizer reports in $build/litfuzz.log" >&2
	fail=1
fi
[ $fail = 0 ] && echo "litfuzz: no differences"
exit $fail
