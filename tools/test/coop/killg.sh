#!/bin/bash
# ends the ScrapC processes whose game folder is <scratch>/g-<name> (several names); a rendered game that
# ignores SIGTERM (gamescope on the RTX) gets SIGKILL after 3 s
. "$(dirname "$(readlink -f "$0")")/env.sh"
pids=""
for n in "$@"; do
	for p in $(pgrep -x ScrapC); do
		case "$(readlink /proc/$p/cwd)" in "$S/g-$n/Bin"|"$S/g-$n") kill "$p"; pids="$pids $p";; esac
	done
done
[ -z "$pids" ] && exit 0
end=$((SECONDS + 3))
while [ $SECONDS -lt $end ]; do
	alive=""
	for p in $pids; do kill -0 "$p" 2>/dev/null && alive="$alive $p"; done
	[ -z "$alive" ] && exit 0
	timeout 0.3 tail -f /dev/null
done
for p in $alive; do kill -9 "$p" 2>/dev/null; done
exit 0
