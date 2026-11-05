#!/bin/bash
# The level test of the 64-bit Linux build: every playable level runs headless and muted for a while in the
# AddressSanitizer build, one line per level. A level fails on an ASan error, a fatal error of the game (its message
# box text in scrapdecomp.log) or a Python traceback. Exit code 0 when all pass.
#
# usage: tools/test/levels.sh [level ...]   (default all 29; builds nothing: run cmake --build build-linux64-asan first)
#        LEVELS_TIME      seconds per level, default 120
#        SCRAP_BUILD      the ASan build folder, default build-linux64-asan (CMAKE_C_FLAGS=-fsanitize=address
#                         -fno-omit-frame-pointer, FFmpeg as in build-linux64)
#        SCRAP_GAME_DIR   the game folder, default <build>/game (made by mkgame.sh --native)
#        LEVELS_JOBS      levels at the same time, default 4; worker n > 0 uses the game folder <game>-<n>
# The log of a level is <game folder of its worker>/levels-<level>.log. The lines come in the order of the levels.
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${SCRAP_BUILD:-$root/build-linux64-asan}
game=${SCRAP_GAME_DIR:-$build/game}
levels=("$@")
[ ${#levels[@]} -gt 0 ] || levels=(AB ABIndoor Bank CA Commercial DownTown FZ GamblinDen GDB Junkyard MCA MCB MDA MDB Menu MOA
	MOB MSA MSB OM Orbit Outskirts Police Press QF Scrapyard ShipEdit Temple Townhall)
jobs=${LEVELS_JOBS:-4}
out=$(mktemp -d) || exit 1
trap 'rm -rf "$out"' EXIT

# level <name> <game folder>: one line into $out/<name>
level() {
	local L=$1 g=$2 log=$2/levels-$1.log rc asan fatal py
	SCRAP_BUILD=$build SCRAP_GAME_DIR=$g HEADLESS_TIMEOUT=${LEVELS_TIME:-120} NATIVE_LOG=$log \
		ASAN_OPTIONS=detect_leaks=0:quarantine_size_mb=2048${ASAN_EXTRA:+:$ASAN_EXTRA} "$root/tools/test/native.sh" -imap:Levels/"$L" > /dev/null 2>&1
	rc=$?
	asan=$(grep -A40 "ERROR: AddressSanitizer" "$log" | grep -E "ERROR|#[0-9]+ 0x[0-9a-f]+ in [A-Za-z]" |
		grep -v "libasan\|libc.so" | head -4 | sed 's/.* in //' | tr '\n' ';' | cut -c1-200)
	fatal=$(grep -a -m1 -A1 "message box" "$g/Bin/scrapdecomp.log" | tr '\n' ' ' | cut -c1-150)
	py=$(grep -ac "Traceback" "$g/Bin/scrapdecomp.log")
	# rc 124: alive until the timeout, the normal end
	if [ "$rc" = 124 ] && [ -z "$asan" ] && [ -z "$fatal" ] && [ "$py" = 0 ]; then
		printf '%-11s ok\n' "$L" > "$out/$L"
	else
		printf '%-11s FAIL rc=%s tracebacks=%s %s %s\n' "$L" "$rc" "$py" "$asan" "$fatal" > "$out/$L"
	fi
}

# worker n takes the levels n, n + jobs, ...
for ((n = 0; n < jobs; n++)); do
	g=$game
	[ $n -gt 0 ] && g=$game-$n
	(for ((i = n; i < ${#levels[@]}; i += jobs)); do level "${levels[i]}" "$g"; done) &
done

fail=0
for L in "${levels[@]}"; do
	while [ ! -s "$out/$L" ] && [ -n "$(jobs -r)" ]; do
		sleep 2
	done
	if [ -s "$out/$L" ]; then
		cat "$out/$L"
		grep -q " ok$" "$out/$L" || fail=1
	else
		printf '%-11s FAIL no result\n' "$L"
		fail=1
	fi
done
wait
exit $fail
