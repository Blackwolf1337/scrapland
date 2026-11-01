#!/bin/bash
# Creates the project Wine prefix (prefix/), switches its sound off for good and sets the language of the game. Safe
# to run again.
root=$(cd "$(dirname "$0")/../.." && pwd)
export WINEPREFIX=$root/prefix WINEDEBUG=-all
[ -d "$WINEPREFIX/drive_c" ] || HEADLESS_TIMEOUT=180 "$root/tools/test/wine.sh" wineboot -i || exit 1
wine reg add 'HKCU\Software\Wine\Drivers' /v Audio /t REG_SZ /d '' /f > /dev/null || exit 1
# the C has no GOG code and takes the language from here only (else the default, French); Scrap.exe reads it too and
# then takes the one of GOG, which is english in the test setup
wine reg add 'HKLM\Software\MercurySteam Entertainment\Scrapland' /v LANGUAGE /t REG_SZ /d english /f /reg:32 \
	> /dev/null || exit 1
wineserver -w
