#!/bin/bash
# The check of the native values of the script ports (src/game/Script/ScriptNative.c) against the linked Python 1.5.2:
# the game runs in the main menu, hidden and muted, with the test channel; tools/test/litfuzz.py runs in it through
# the channel's py command and compares the native literal reader, repr, str and check() texts with Script_Literal,
# eval, repr and str of 1.5.2 (random texts per seed, once with the atoms of the nopy-4 fuzz and once with more), and
# the repr cases of every type copied into native values. The game folder is <build>/litfuzz (made by mkgame.sh
# --native). Prints one line per run plus the first differences; the exit code is 0 when there are none (and, in an
# ASan build, no AddressSanitizer report in <build>/litfuzz.log).
#
# usage: tools/test/litfuzz.sh [texts per seed, default 3200] [seeds ..., default 1 2 3 4 5]
#        SCRAP_BUILD      the build folder, default build-linux64 (build-linux64-asan for ASan); not one built
#                         without the interpreter (SCRAP_NO_PYTHON: there is no Python to compare with)
#        LITFUZZ_TIMEOUT  seconds for the whole run, default 1200
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "${SCRAP_BUILD:-$root/build-linux64}" && pwd) || exit 1
game=$build/litfuzz
timeout=${LITFUZZ_TIMEOUT:-1200}
count=${1:-3200}
shift
seeds=("$@")
[ ${#seeds[@]} -gt 0 ] || seeds=(1 2 3 4 5)
if grep -qx 'SCRAP_NO_PYTHON:BOOL=ON' "$build/CMakeCache.txt" 2> /dev/null; then
	echo "litfuzz: $build has no Python interpreter to compare with" >&2
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
out=$(python3 "$root/tools/test/channel.py" "$game" --wait 300 > /dev/null && channel 'native py')
if [ "$out" != ok ]; then
	echo "litfuzz: the game did not answer or has no native command: $out" >&2
	fail=1
else
	results=$(channel py "execfile('litfuzz.py')" 'result = reprs()')
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
fi
endgame
wait "$pid" 2> /dev/null
if grep -q AddressSanitizer "$build/litfuzz.log" 2> /dev/null; then
	echo "litfuzz: AddressSanitizer reports in $build/litfuzz.log" >&2
	fail=1
fi
[ $fail = 0 ] && echo "litfuzz: no differences"
exit $fail
