#!/bin/bash
# screenshot of the gamescope a ScrapC runs in, found by its game folder: shot.sh <cwd pattern> <out.png>
# (the scrap MCP may pick another agent's gamescope socket when several start at once)
pat=$1 out=$2
for p in $(pgrep -x ScrapC); do
	case "$(readlink /proc/$p/cwd)" in
	*/$pat|*/$pat/*)
		d=$(tr '\0' '\n' < /proc/$p/environ | sed -n 's/^GAMESCOPE_WAYLAND_DISPLAY=//p')
		rm -f "$out"
		GAMESCOPE_WAYLAND_DISPLAY=$d gamescopectl screenshot "$out" > /dev/null 2>&1
		for i in $(seq 1 30); do [ -s "$out" ] && break; timeout 0.2 tail -f /dev/null; done
		[ -s "$out" ] && echo "$out ($d)" && exit 0
		echo "no screenshot from $d"; exit 1;;
	esac
done
echo "no game for $pat"; exit 1
