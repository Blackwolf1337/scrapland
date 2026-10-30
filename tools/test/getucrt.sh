#!/bin/bash
# Puts Microsoft's ucrtbase.dll (from the VC++ 2015-2019 redistributable, the URL and hash winetricks uses) as
# game/Bin/ucrtbase_ms.dll. ScrapC.exe takes only the MSVC math from it (docs/standalone.md, stage 2b): Wine's own
# ucrtbase differs from the original's static UCRT in the last bits. ScrapC.exe runs without it (then with Wine's
# math); on Windows it uses the system ucrtbase.dll and this file is not needed. Not in git (Microsoft binary).
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
dst=$root/game/Bin/ucrtbase_ms.dll
[ -f "$dst" ] && { echo "getucrt.sh: $dst is there"; exit 0; }
url=https://download.visualstudio.microsoft.com/download/pr/85d47aa9-69ae-4162-8300-e6b7e4bf3cf3/14563755AC24A874241935EF2C22C5FCE973ACB001F99E524145113B2DC638C1/VC_redist.x86.exe
sum=14563755ac24a874241935ef2c22c5fce973acb001f99e524145113b2dc638c1
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -sSfL -o "$tmp/redist.exe" "$url"
echo "$sum  $tmp/redist.exe" | sha256sum -c --quiet
cabextract -q -d "$tmp" -F a10 "$tmp/redist.exe"
cabextract -q -d "$tmp" -F ucrtbase.dll "$tmp/a10" 2> /dev/null
cp "$tmp/ucrtbase.dll" "$dst.new" && mv "$dst.new" "$dst"
echo "getucrt.sh: $dst"
