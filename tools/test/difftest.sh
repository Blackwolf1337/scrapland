#!/bin/bash
# Differential test of ScrapC.exe: runs the test scripts of tools/test/py in the game, once with the current build and
# once with a frozen reference ScrapC.exe, and compares the outputs. Hidden and muted through wine.sh. Exit code 0
# when every run finished and the outputs match.
#
# Scenarios: sdtest.py in the main menu, sdcall.py there too (functions called by name through the module SDTest of
# src/core/testmod.c), sdlevel.py in the multiplayer level FZ (-imap:Levels/FZ). Each scenario runs in a game of its
# own (build/difftest/slot-<scenario>, made once by mkgame.sh --into), so they run at the same time. The outputs of
# the reference are cached by the hashes of the reference exe, the test scripts and the scenario arguments: the
# reference does not change, so after the first run only the current build runs.
#
# usage: tools/test/difftest.sh [scenario ...]   (sdtest, sdcall, sdlevel; default all; builds nothing: run cmake first)
#        DIFFTEST_REF      the reference exe, default build-ref-noexact/ScrapC.exe (tag ref-noexact)
#        DIFFTEST_TIMEOUT  seconds per run, default 240
#        DIFFTEST_JOBS     scenarios at the same time, default 3
#        DIFFTEST_NOCACHE=1  run the reference even when its outputs are cached
#        DIFFTEST_NATIVE=1   the current build is the native Linux one (build-linux/ScrapC, through native.sh in the
#                            same game folder of the slot, so the paths are the same); the reference stays the Windows
#                            exe in Wine. The outputs are compared without the CR of the Windows line ends. The
#                            differences of the two systems that were reviewed (libm last bits, qsort ties, the
#                            uninitialized memory and heap pointers some checks print, the paths with other drives)
#                            are in tools/test/native-accepted/<scenario>.txt by the names of their checks (the part
#                            before " = "; the values of some change from run to run): only other checks fail.
#                            DIFFTEST_ACCEPT=1 writes the differences of this run as the new accepted list.
root=$(cd "$(dirname "$0")/../.." && pwd)
ref=${DIFFTEST_REF:-$root/build-ref-noexact/ScrapC.exe}
out=$root/build/difftest
cache=$out/refcache
mkdir -p "$out" "$cache"

declare -A extra=([sdtest]="" [sdcall]="" [sdlevel]="-imap:Levels/FZ")
scenarios=("$@")
[ ${#scenarios[@]} -gt 0 ] || scenarios=(sdtest sdcall sdlevel)
for s in "${scenarios[@]}"; do
	[ -n "${extra[$s]+x}" ] || { echo "difftest.sh: unknown scenario $s" >&2; exit 1; }
done
[ -f "$ref" ] || { echo "difftest.sh: no reference exe $ref" >&2; exit 1; }
native=${DIFFTEST_NATIVE:-0}
if [ "$native" = 1 ]; then
	[ -f "$root/build-linux/ScrapC" ] || { echo "difftest.sh: build-linux/ScrapC missing, build first" >&2; exit 1; }
else
	[ -f "$root/build/ScrapC.exe" ] || { echo "difftest.sh: build/ScrapC.exe missing, build first" >&2; exit 1; }
fi
python3 "$root/tools/test/sigcheck.py" "$root"/tools/test/py/*.py || exit 1
"$root/tools/test/install.sh" > /dev/null || exit 1
scripts=$(cat "$root"/tools/test/py/*.py | sha256sum | cut -c1-16)
refsum=$(sha256sum "$ref" | cut -c1-16)

# the files the scripts write into the game directory
rm_written() {
	shopt -s nocaseglob
	rm -f "$1"/sd*.txt "$1"/sd*.txf "$1"/sd*.bmp "$1"/sd*.tga "$1"/sd*.tex "$1"/sd*.bak
	rm -rf "$1"/sdbsctxf
	shopt -u nocaseglob
}

# the slot of a scenario: its game and prefix, the exes, DLLs and scripts of now
slot_prepare() {
	local dir=$out/slot-$1
	if [ ! -d "$dir/game/Bin" ] || [ ! -d "$dir/prefix" ]; then
		"$root/tools/test/mkgame.sh" --into "$dir" "$root" > /dev/null || return 1
	fi
	cp "$root/game/Bin/ScrapC.exe" "$root"/game/Bin/av*.dll "$root"/game/Bin/swscale*.dll "$root"/game/Bin/SDL3.dll \
		"$dir/game/Bin/" || return 1
	cp "$ref" "$dir/game/Bin/ScrapRef.exe" || return 1
	rm -f "$dir"/game/sd*.py "$dir"/game/sd*.out
	cp "$root"/tools/test/py/*.py "$dir/game/" || return 1
	printf '[general]\ntestmod=1\n' > "$dir/game/Bin/scrapdecomp.ini"
	if [ "$native" = 1 ]; then
		# the configuration and the saves of native.sh (XDG_DATA_HOME), fresh from the prefix like the Wine runs see
		rm -rf "$dir/game/xdg-data"
		mkdir -p "$dir/game/xdg-data/MercurySteam"
		cp -a "$dir/prefix/drive_c/users/claude/Documents/MercurySteam/Scrapland" "$dir/game/xdg-data/MercurySteam/" ||
			return 1
	fi
}

# the native games of a game folder: killed, as wineserver -k kills the Windows ones (a quit would save the
# configuration the scripts changed)
native_stop() {
	local exe=$1/Bin/ScrapC p
	for p in /proc/[0-9]*; do
		[ "$(readlink "$p/exe" 2> /dev/null)" = "$exe" ] && kill -9 "${p#/proc/}" 2> /dev/null
	done
}

# run <scenario> <exe> <tag>: one game in the slot of the scenario, its output in $out/<tag>.out
run() {
	local name=$1 exe=$2 tag=$3 dir=$out/slot-$1
	local game=$dir/game result=$dir/game/$1.out
	local arg
	arg="-PythonExecute:execfile($(python3 -I -c "print('+'.join('chr(%d)' % ord(c) for c in '$name.py'))"))"
	rm -f "$result"
	rm_written "$game"
	if [ "$exe" = native ]; then
		# shellcheck disable=SC2086 # extra arguments of the scenario, word split on purpose
		SCRAP_GAME_DIR=$game HEADLESS_TIMEOUT=${DIFFTEST_TIMEOUT:-240} NATIVE_LOG="$out/native-$tag.log" \
			"$root/tools/test/native.sh" -fullscreen:0 ${extra[$name]} "$arg" &
	else
		# shellcheck disable=SC2086 # extra arguments of the scenario, word split on purpose
		SCRAP_GAME_DIR=$game SCRAP_PREFIX=$dir/prefix HEADLESS_TIMEOUT=${DIFFTEST_TIMEOUT:-240} \
			WINEDLLOVERRIDES="dinput8=b" WINE_LOG="$out/wine-$tag.log" \
			"$root/tools/test/wine.sh" "$exe" -fullscreen:0 ${extra[$name]} "$arg" &
	fi
	local pid=$!
	# the script writes "done" as its last line (CRLF when Python runs on Windows); then the game is stopped
	# (Scrap.Exit() from the startup script did not quit reliably)
	while kill -0 "$pid" 2> /dev/null; do
		if tail -n 1 "$result" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
			if [ "$exe" = native ]; then
				native_stop "$game"
			else
				WINEPREFIX=$dir/prefix wineserver -k 2> /dev/null
			fi
			break
		fi
		sleep 1
	done
	wait "$pid" 2> /dev/null
	cp "$game/Bin/scrapdecomp.log" "$out/scrapdecomp-$tag.log" 2> /dev/null
	cp "$game/log.html" "$out/log-$tag.html" 2> /dev/null
	# a crash after the last check leaves the output complete: only the game's log shows it
	if grep -q "Unhandled exception" "$out/log-$tag.html" 2> /dev/null; then
		echo "$tag: unhandled exception in the game log, see $out/log-$tag.html" >&2
	fi
	cp "$result" "$out/$tag.out" 2> /dev/null
	rm_written "$game"
	if ! tail -n 1 "$result" 2> /dev/null | tr -d '\r' | grep -qx 'done'; then
		echo "$tag: output incomplete, last lines in $out/$tag.out" >&2
		return 1
	fi
}

# scenario <name>: the current build, the reference (or its cached output), the comparison; the summary line in
# $out/<name>.result, the exit code 0 when they match
scenario() {
	local s=$1 c o=0
	local key
	key=$cache/$refsum-$scripts-$(printf '%s' "${extra[$s]}" | sha256sum | cut -c1-8)
	slot_prepare "$s" || { echo "$s: the slot could not be made" > "$out/$s.result"; return 1; }
	if [ "$native" = 1 ]; then
		run "$s" native "$s-c"; c=$?
	else
		run "$s" ScrapC.exe "$s-c"; c=$?
	fi
	if [ "${DIFFTEST_NOCACHE:-0}" != 1 ] && [ -f "$key-$s.out" ]; then
		cp "$key-$s.out" "$out/$s-orig.out"
	else
		run "$s" ScrapRef.exe "$s-orig"; o=$?
		[ $o -eq 0 ] && cp "$out/$s-orig.out" "$key-$s.out"
	fi
	if [ $c -ne 0 ] || [ $o -ne 0 ]; then
		echo "$s: incomplete" > "$out/$s.result"
		return 1
	fi
	local a=$out/$s-orig.out b=$out/$s-c.out
	if [ "$native" = 1 ]; then
		# the NaN text of the two runtimes ("-nan(ind)" of Windows, "-nan" of glibc) made the same
		a=$out/$s-orig.norm b=$out/$s-c.norm
		sed -E 's/-?nan\((ind|snan)\)|-nan\b/nan/g' "$out/$s-orig.out" > "$a"
		sed -E 's/-?nan\((ind|snan)\)|-nan\b/nan/g' "$out/$s-c.out" > "$b"
	fi
	if diff -u --strip-trailing-cr "$a" "$b" > "$out/$s.diff"; then
		echo "$s: $(($(wc -l < "$out/$s-c.out") - 1)) checks match" \
			"($(grep -c ' = exception ' "$out/$s-c.out") raise an exception in both)" > "$out/$s.result"
		return 0
	fi
	if [ "$native" = 1 ]; then
		local accepted=$root/tools/test/native-accepted/$s.txt
		grep '^[-+][^-+]' "$out/$s.diff" | sed -E 's/^[-+]//; s/ = .*//' | LC_ALL=C sort -u > "$out/$s.lines"
		if [ "${DIFFTEST_ACCEPT:-0}" = 1 ]; then
			mkdir -p "$root/tools/test/native-accepted"
			cp "$out/$s.lines" "$accepted"
		fi
		LC_ALL=C comm -23 "$out/$s.lines" "$accepted" 2> /dev/null > "$out/$s.new" ||
			cp "$out/$s.lines" "$out/$s.new"
		if [ -f "$accepted" ] && [ ! -s "$out/$s.new" ]; then
			echo "$s: $(($(wc -l < "$out/$s-c.out") - 1)) checks match except" \
				"$(wc -l < "$out/$s.lines") reviewed native ones" > "$out/$s.result"
			return 0
		fi
		echo "$s: $(wc -l < "$out/$s.new") more checks differ, see $out/$s.new and $out/$s.diff" > "$out/$s.result"
		return 1
	fi
	echo "$s: outputs differ, see $out/$s.diff" > "$out/$s.result"
	return 1
}

rc=0
pids=()
for s in "${scenarios[@]}"; do
	while [ "$(jobs -rp | wc -l)" -ge "${DIFFTEST_JOBS:-3}" ]; do
		wait -n
	done
	rm -f "$out/$s.result"
	scenario "$s" &
	pids+=($!)
done
for p in "${pids[@]}"; do
	wait "$p" || rc=1
done
for s in "${scenarios[@]}"; do
	cat "$out/$s.result" 2> /dev/null || { echo "$s: no result" >&2; rc=1; }
done
exit $rc
