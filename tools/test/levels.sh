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
# The log of a level is <game>/levels-<level>.log.
root=$(cd "$(dirname "$0")/../.." && pwd)
build=${SCRAP_BUILD:-$root/build-linux64-asan}
game=${SCRAP_GAME_DIR:-$build/game}
levels=("$@")
[ ${#levels[@]} -gt 0 ] || levels=(AB ABIndoor Bank CA Commercial DownTown FZ GamblinDen GDB Junkyard MCA MCB MDA MDB Menu MOA
	MOB MSA MSB OM Orbit Outskirts Police Press QF Scrapyard ShipEdit Temple Townhall)
fail=0
for L in "${levels[@]}"; do
	log=$game/levels-$L.log
	SCRAP_BUILD=$build SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=${LEVELS_TIME:-120} NATIVE_LOG=$log \
		ASAN_OPTIONS=detect_leaks=0:quarantine_size_mb=2048 "$root/tools/test/native.sh" -imap:Levels/"$L" > /dev/null 2>&1
	rc=$?
	asan=$(grep -A40 "ERROR: AddressSanitizer" "$log" | grep -E "ERROR|#[0-9]+ 0x[0-9a-f]+ in [A-Za-z]" |
		grep -v "libasan\|libc.so" | head -4 | sed 's/.* in //' | tr '\n' ';' | cut -c1-200)
	fatal=$(grep -a -m1 -A1 "message box" "$game/Bin/scrapdecomp.log" | tr '\n' ' ' | cut -c1-150)
	py=$(grep -ac "Traceback" "$game/Bin/scrapdecomp.log")
	# rc 124: alive until the timeout, the normal end
	if [ "$rc" = 124 ] && [ -z "$asan" ] && [ -z "$fatal" ] && [ "$py" = 0 ]; then
		printf '%-11s ok\n' "$L"
	else
		printf '%-11s FAIL rc=%s tracebacks=%s %s %s\n' "$L" "$rc" "$py" "$asan" "$fatal"
		fail=1
	fi
done
exit $fail
