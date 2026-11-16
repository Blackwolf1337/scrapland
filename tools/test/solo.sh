#!/bin/bash
# Runs an input tape (include/scrap/inputtape.h, recorded with tools/test/tape.py rec) in single player and on the
# solo instance, both in the deterministic test mode with the trace, and prints the first line of the player's state
# (the "pl" lines of the traces) that differs, or "equal", with the first differing slot per field (tape.py cmp).
# The single player replay is checked first: it must give the controls of the tape (the console line "inputtape:
# the controls of slot ... differ" is printed when not).
#
# usage: tools/test/solo.sh <level> <tape> [secs]
#        secs: the time limit of each run in seconds (default 900)
#        SOLO_OUT   folder for the traces and logs (default the folder of the tape), SCRAP_BUILD the build folder
#        (default build-linux64 of this checkout), SOLO_VK the GPU of the single player run (default 10de:27b0)
# Exit code 0 when the two runs are equal, 1 when they differ, 2 when a run failed.
root=$(cd "$(dirname "$0")/../.." && pwd)
[ $# -ge 2 ] || { sed -n '2,13p' "$0" >&2; exit 2; }
level=$1
tape=$(realpath "$2") || exit 2
secs=${3:-900}
out=${SOLO_OUT:-$(dirname "$tape")}
build=${SCRAP_BUILD:-$root/build-linux64}
vk=${SOLO_VK:-10de:27b0}
base=$out/$(basename "$tape" .tape)
mkdir -p "$out" || exit 2
rm -f "$base-sp.trace" "$base-solo.trace"
python3 "$root/tools/test/tape.py" play "$level" "$tape" --trace "$base-sp.trace" --build "$build" --vk "$vk" \
	--timeout "$secs" --name "solo-sp-$level" || exit 2
python3 "$root/tools/test/tape.py" solo "$level" "$tape" --trace "$base-solo.trace" --build "$build" \
	--timeout "$secs" --name "solo-inst-$level" || exit 2
[ -s "$base-sp.trace" ] && [ -s "$base-solo.trace" ] || { echo "solo.sh: a run wrote no trace" >&2; exit 2; }
grep -h "inputtape:" "$root/build-mcp/slot-solo-sp-$level/wine.log" "$root/build-mcp/slot-solo-inst-$level/solo.log" \
	2> /dev/null
python3 "$root/tools/test/tape.py" cmp "$base-sp.trace" "$base-solo.trace"
