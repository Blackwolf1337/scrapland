#!/bin/bash
# ends a world and its players: stopw.sh <tag> [player names...]; the coordinator quits (its instances stop by -stopinstances
# or by their quit files), then whatever still runs in the game folders is killed
. "$(dirname "$(readlink -f "$0")")/env.sh"
tag=$1; shift
"$T/killg.sh" "$@"
if [ -s "$S/$tag.pid" ] && kill -0 "$(cat "$S/$tag.pid")" 2>/dev/null; then
	"$T/send.sh" "$tag" quit
	"$T/waitpid.sh" "$(cat "$S/$tag.pid")" 20 || kill "$(cat "$S/$tag.pid")" 2>/dev/null
fi
touch "$S/g-world-$tag"/instance_{Orbit,Fake,Junkyard,AB,ABIndoor,QF,OM,FZ,CA,DownTown,Outskirts,Police,Bank,Press,Temple,TownHall,GamblinDen,Commercial,Scrapyard,GDB}.quit 2>/dev/null
end=$((SECONDS + 15))
while [ $SECONDS -lt $end ]; do
	n=0
	for p in $(pgrep -x ScrapC); do
		case "$(readlink /proc/$p/cwd)" in "$S/g-world-$tag/Bin") n=$((n + 1));; esac
	done
	[ $n = 0 ] && break
	timeout 0.5 tail -f /dev/null
done
"$T/killg.sh" "world-$tag"
rm -f "$S/g-world-$tag"/instance_*.quit
