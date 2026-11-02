#!/bin/bash
# Runs ScrapC.exe with the Vulkan validation layer for a while (through wine.sh, hidden and muted) and prints the
# validation messages that are ours: those of gamescope and Wine (tools/test/vk-foreign-vuids.txt) are left out.
# Exit code 0 when none is left, 1 else.
#
# usage: tools/test/vkcheck.sh [seconds] [arguments of the game]   (default 40 s, e.g. 60 -imap:Levels/FZ)
root=$(cd "$(dirname "$0")/../.." && pwd)
secs=${1:-40}
shift
log=$root/build/vkcheck.log
mkdir -p "$root/build"
"$root/tools/test/install.sh" > /dev/null || exit 1
SCRAP_VK_VALIDATION=1 WINE_LOG="$log" HEADLESS_TIMEOUT=$secs \
	"$root/tools/test/wine.sh" ScrapC.exe -fullscreen:0 "$@" 2> /dev/null
foreign=$(grep -v '^#' "$root/tools/test/vk-foreign-vuids.txt" | cut -f1 | grep -v '^$')
ours=$(grep -o 'VUID-[A-Za-z0-9_-]*' "$log" | grep -vxF "$foreign" | sort | uniq -c | sort -rn)
total=$(grep -c 'Validation Error' "$log")
if [ -z "$ours" ]; then
	echo "vkcheck: no validation errors of ours ($total of gamescope/Wine left out, log $log)"
	exit 0
fi
echo "vkcheck: validation errors of ours (log $log):"
while read -r n vuid; do
	echo "$n x $vuid"
	grep -m1 -A1 -F "[ $vuid ]" "$log" | tail -n 1 | cut -c1-300 | sed 's/^/    /'
done <<< "$ours"
exit 1
