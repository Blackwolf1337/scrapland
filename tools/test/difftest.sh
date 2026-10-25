#!/bin/bash
# Differential test of the C functions: runs tools/test/py/sdtest.py in the game twice, with the hooks of the DLL
# ([general] hooks=1) and with the original functions (hooks=0), and compares the outputs. Hidden and muted
# through wine.sh. Exit code 0 when both runs finished and match.
#
# usage: tools/test/difftest.sh       (builds nothing: run cmake --build build first)
#        DIFFTEST_TIMEOUT  seconds per run, default 90
root=$(cd "$(dirname "$0")/../.." && pwd)
game=$root/game
ini=$game/Bin/scrapdecomp.ini
out=$root/build/difftest
mkdir -p "$out"
fifo=$(mktemp -u)
mkfifo "$fifo" && exec 3<> "$fifo" && rm -f "$fifo"
"$root/tools/test/install.sh" || exit 1
cp "$root/tools/test/py/sdtest.py" "$game/sdtest.py" || exit 1
arg="-PythonExecute:execfile($(python3 -I -c "print('+'.join('chr(%d)' % ord(c) for c in 'sdtest.py'))"))"

run() {
	printf '[general]\nhooks=%s\n' "$1" > "$ini"
	rm -f "$game/sdtest.out"
	HEADLESS_TIMEOUT=${DIFFTEST_TIMEOUT:-90} WINEDLLOVERRIDES="dinput8=n,b" WINE_LOG="$out/wine-$2.log" \
		"$root/tools/test/wine.sh" Scrap.exe -fullscreen:0 "$arg" &
	local pid=$!
	# the script writes "done" as its last line (CRLF, Python runs on Windows); then the game is stopped (Scrap.Exit() from the startup script
	# did not quit reliably)
	while kill -0 "$pid" 2> /dev/null; do
		if tail -n 1 "$game/sdtest.out" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
			WINEPREFIX=$root/prefix wineserver -k 2> /dev/null
			break
		fi
		read -rt 1 -u 3 # one second, without using CPU: nobody writes to the fifo
	done
	wait "$pid" 2> /dev/null
	cp "$game/Bin/scrapdecomp.log" "$out/scrapdecomp-$2.log" 2> /dev/null
	cp "$game/log.html" "$out/log-$2.html" 2> /dev/null
	if ! tail -n 1 "$game/sdtest.out" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
		echo "$2 run: sdtest.out incomplete, see $out" >&2
		return 1
	fi
	cp "$game/sdtest.out" "$out/sdtest-$2.out"
}

run 1 c && run 0 orig
rc=$?
rm -f "$ini" "$game/sdtest.py" "$game/sdtest.out"
[ $rc -eq 0 ] || exit 1
grep 'hooks installed' "$out/scrapdecomp-c.log"
if diff -u "$out/sdtest-orig.out" "$out/sdtest-c.out"; then
	echo "difftest: $(($(wc -l < "$out/sdtest-c.out") - 1)) checks, C functions match the originals"
else
	echo "difftest: outputs differ" >&2
	exit 1
fi
