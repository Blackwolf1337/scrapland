#!/bin/bash
# Differential test of the C functions: runs the test scripts of tools/test/py in the game, each twice, with the
# hooks of the DLL ([general] hooks=1) and with the original functions (hooks=0), and compares the outputs.
# Hidden and muted through wine.sh. Exit code 0 when every run finished and the outputs match.
#
# Scenarios: sdtest.py in the main menu, sdcall.py there too (pure functions called by RVA through the module
# SDTest of src/core/testmod.c), sdlevel.py in the multiplayer level FZ (-imap:Levels/FZ).
#
# usage: tools/test/difftest.sh [scenario ...]   (sdtest, sdcall, sdlevel; default all; builds nothing: run cmake first)
#        DIFFTEST_TIMEOUT  seconds per run, default 90
root=$(cd "$(dirname "$0")/../.." && pwd)
game=$root/game
ini=$game/Bin/scrapdecomp.ini
out=$root/build/difftest
mkdir -p "$out"
fifo=$(mktemp -u)
mkfifo "$fifo" && exec 3<> "$fifo" && rm -f "$fifo"

declare -A extra=([sdtest]="" [sdcall]="" [sdlevel]="-imap:Levels/FZ")
scenarios=("$@")
[ ${#scenarios[@]} -gt 0 ] || scenarios=(sdtest sdcall sdlevel)

"$root/tools/test/install.sh" || exit 1
cp "$root"/tools/test/py/*.py "$game/" || exit 1

# run <scenario> <hooks 1|0> <tag>
run() {
	local name=$1 result=$game/$1.out tag=$1-$3
	local arg
	arg="-PythonExecute:execfile($(python3 -I -c "print('+'.join('chr(%d)' % ord(c) for c in '$name.py'))"))"
	printf '[general]\nhooks=%s\ntestmod=1\n' "$2" > "$ini"
	rm -f "$result"
	# shellcheck disable=SC2086 # extra arguments of the scenario, word split on purpose
	HEADLESS_TIMEOUT=${DIFFTEST_TIMEOUT:-90} WINEDLLOVERRIDES="dinput8=n,b" WINE_LOG="$out/wine-$tag.log" \
		"$root/tools/test/wine.sh" Scrap.exe -fullscreen:0 ${extra[$name]} "$arg" &
	local pid=$!
	# the script writes "done" as its last line (CRLF, Python runs on Windows); then the game is stopped
	# (Scrap.Exit() from the startup script did not quit reliably)
	while kill -0 "$pid" 2> /dev/null; do
		if tail -n 1 "$result" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
			WINEPREFIX=$root/prefix wineserver -k 2> /dev/null
			break
		fi
		read -rt 1 -u 3 # one second, without using CPU: nobody writes to the fifo
	done
	wait "$pid" 2> /dev/null
	cp "$game/Bin/scrapdecomp.log" "$out/scrapdecomp-$tag.log" 2> /dev/null
	cp "$game/log.html" "$out/log-$tag.html" 2> /dev/null
	cp "$result" "$out/$tag.out" 2> /dev/null
	if ! tail -n 1 "$result" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
		echo "$tag: output incomplete, last lines in $out/$tag.out" >&2
		return 1
	fi
}

rc=0
for s in "${scenarios[@]}"; do
	# both runs even if one stops: the last lines show whether the original stops at the same check
	run "$s" 1 c; c=$?
	run "$s" 0 orig; o=$?
	if [ $c -ne 0 ] || [ $o -ne 0 ]; then
		rc=1
		continue
	fi
	if diff -u "$out/$s-orig.out" "$out/$s-c.out" > "$out/$s.diff"; then
		echo "$s: $(($(wc -l < "$out/$s-c.out") - 1)) checks match" \
			"($(grep -c ' = exception ' "$out/$s-c.out") raise an exception in both)"
	else
		echo "$s: outputs differ, see $out/$s.diff" >&2
		rc=1
	fi
done
grep -h 'hooks installed' "$out"/scrapdecomp-*-c.log | sort -u
rm -f "$ini" "$game"/sd*.py "$game"/sd*.out "$game"/sd*.txt
exit $rc
