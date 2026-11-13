#!/bin/bash
# The check of the native values of the script ports (src/game/Script/ScriptNative.c) against Python 1.5.2: the game
# runs in the main menu, hidden and muted, with the test channel. The game folder is <build>/litfuzz (made by
# mkgame.sh --native). It checks the stored texts, tools/test/litfuzz.expected.xz (tools/test/litstore.py), through the
# channel's native command: the native literal reader must take the texts 1.5.2 took and give their stored repr, str
# and check() texts, and refuse the others. Prints a summary line plus the first differences; the exit code is 0 when
# there are none (and, in an ASan build, no AddressSanitizer report in <build>/litfuzz.log). The store is the final
# reference: the live run against the linked Python and its update went with the interpreter (tag interp-final).
#
# usage: tools/test/litfuzz.sh
#        SCRAP_BUILD      the build folder, default build-linux64 (build-linux64-asan for ASan)
#        LITFUZZ_TIMEOUT  seconds for the whole run, default 1200
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "${SCRAP_BUILD:-$root/build-linux64}" && pwd) || exit 1
game=$build/litfuzz
timeout=${LITFUZZ_TIMEOUT:-1200}
store=$root/tools/test/litfuzz.expected.xz
[ -d "$game/Bin" ] || "$root/tools/test/mkgame.sh" --native "$game" || exit 1
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
fail=0
if ! python3 "$root/tools/test/channel.py" "$game" --wait 300 > /dev/null; then
	echo "litfuzz: the game did not answer" >&2
	fail=1
else
	python3 "$root/tools/test/litstore.py" check "$game" "$store" || fail=1
fi
endgame
wait "$pid" 2> /dev/null
if grep -q AddressSanitizer "$build/litfuzz.log" 2> /dev/null; then
	echo "litfuzz: AddressSanitizer reports in $build/litfuzz.log" >&2
	fail=1
fi
[ $fail = 0 ] && echo "litfuzz: no differences"
exit $fail
