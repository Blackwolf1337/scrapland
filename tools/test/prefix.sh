#!/bin/bash
# Creates the project Wine prefix (prefix/) and switches its sound off for good. Safe to run again.
root=$(cd "$(dirname "$0")/../.." && pwd)
export WINEPREFIX=$root/prefix WINEDEBUG=-all
[ -d "$WINEPREFIX/drive_c" ] || HEADLESS_TIMEOUT=180 "$root/tools/test/wine.sh" wineboot -i || exit 1
wine reg add 'HKCU\Software\Wine\Drivers' /v Audio /t REG_SZ /d '' /f > /dev/null || exit 1
wineserver -w
