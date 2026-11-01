#!/bin/bash
# Differential test of the C functions: runs the test scripts of tools/test/py in the game, each twice, with the
# hooks of the DLL ([general] hooks=1) and with the original functions (hooks=0), and compares the outputs.
# Hidden and muted through wine.sh. Exit code 0 when every run finished and the outputs match.
#
# Scenarios: sdtest.py in the main menu, sdcall.py there too (pure functions called by RVA through the module
# SDTest of src/core/testmod.c), sdlevel.py in the multiplayer level FZ (-imap:Levels/FZ).
#
# usage: tools/test/difftest.sh [scenario ...]   (sdtest, sdcall, sdlevel; default all; builds nothing: run cmake first)
#        DIFFTEST_TIMEOUT  seconds per run, default 240
#        DIFFTEST_WIPE=1   the hooked run of the hook DLL also fills the rest of every hooked original with int3
#                          ([general] wipe), 2 the whole game code but for the hooks, 3 that with a probe that logs each
#                          trap and goes on with the original code (src/core/hook.c)
#        DIFFTEST_STANDALONE=1  the hooked run starts ScrapC.exe, the standalone exe (docs/standalone.md), which has no
#                          original code and ignores wipe
#        DIFFTEST_REF=exe  compare ScrapC.exe with a reference ScrapC.exe (copied to game/Bin/ScrapRef.exe) in place of
#                          Scrap.exe, e.g. build-ref/ScrapC.exe built from the tag hooked-final; both use the builtin
#                          dinput8 of Wine
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

python3 "$root/tools/test/sigcheck.py" "$root"/tools/test/py/*.py || exit 1
"$root/tools/test/install.sh" || exit 1
cp "$root"/tools/test/py/*.py "$game/" || exit 1
if [ -n "$DIFFTEST_REF" ]; then
	cp "$DIFFTEST_REF" "$game/Bin/ScrapRef.exe.new" && mv "$game/Bin/ScrapRef.exe.new" "$game/Bin/ScrapRef.exe" || exit 1
fi

# the files the scripts write
rm_written() {
	shopt -s nocaseglob
	rm -f "$game"/sd*.txt "$game"/sd*.txf "$game"/sd*.bmp "$game"/sd*.tga "$game"/sd*.tex "$game"/sd*.bak
	rm -rf "$game"/sdbsctxf
	shopt -u nocaseglob
}

# run <scenario> <hooks 1|0> <tag>
run() {
	local name=$1 result=$game/$1.out tag=$1-$3 exe=Scrap.exe
	local arg
	local dinput="dinput8=n,b"
	[ "$2" = 1 ] && [ "${DIFFTEST_STANDALONE:-0}" = 1 ] && exe=ScrapC.exe
	if [ -n "$DIFFTEST_REF" ]; then
		exe=ScrapC.exe dinput="dinput8=b"
		[ "$2" = 0 ] && exe=ScrapRef.exe
	fi
	arg="-PythonExecute:execfile($(python3 -I -c "print('+'.join('chr(%d)' % ord(c) for c in '$name.py'))"))"
	printf '[general]\nhooks=%s\ntestmod=1\nwipe=%s\n' "$2" "$(($2 ? ${DIFFTEST_WIPE:-0} : 0))" > "$ini"
	# so the second run does not find the files of the first
	rm -f "$result"
	rm_written
	# shellcheck disable=SC2086 # extra arguments of the scenario, word split on purpose
	HEADLESS_TIMEOUT=${DIFFTEST_TIMEOUT:-240} WINEDLLOVERRIDES="$dinput" WINE_LOG="$out/wine-$tag.log" \
		"$root/tools/test/wine.sh" "$exe" -fullscreen:0 ${extra[$name]} "$arg" &
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
	# a crash after the last check leaves the output complete: only the game's log shows it
	if grep -q "Unhandled exception" "$out/log-$tag.html" 2> /dev/null; then
		echo "$tag: unhandled exception in the game log, see $out/log-$tag.html" >&2
	fi
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
rm -f "$ini" "$game"/sd*.py "$game"/sd*.out
rm_written
exit $rc
