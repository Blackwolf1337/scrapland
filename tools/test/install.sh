#!/bin/bash
# Copies the built dinput8.dll into game/Bin. Refuses while the game runs; copies to .new and renames, so a
# reader never sees a half written file.
root=$(cd "$(dirname "$0")/../.." && pwd)
src=$root/build/dinput8.dll
dst=$root/game/Bin/dinput8.dll
if pgrep -x 'Scrap\.exe|Dedicated\.exe' > /dev/null; then
	echo "install.sh: the game is running, not copying" >&2
	exit 1
fi
[ -f "$src" ] || { echo "install.sh: $src missing, build first" >&2; exit 1; }
cp "$src" "$dst.new" && mv "$dst.new" "$dst" && echo "installed $dst"
