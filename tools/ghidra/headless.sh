#!/bin/bash
# Runs a script from tools/ghidra on the project database ghidra/Scrapland (Scrap.exe), without new analysis.
# Ghidra 12 needs Java 21 or newer, the system default may be older.
#
# usage: tools/ghidra/headless.sh <Script.java> [script arguments]
#        GHIDRA_DIR   Ghidra install, default /opt/ghidra
#        GHIDRA_JAVA  JDK, default the newest one in /usr/lib/jvm
root=$(cd "$(dirname "$0")/../.." && pwd)
ghidra=${GHIDRA_DIR:-/opt/ghidra}
export JAVA_HOME=${GHIDRA_JAVA:-$(find /usr/lib/jvm -maxdepth 1 -name 'java-*-openjdk' | sort -V | tail -1)}
# a project made under another user name (another machine) opens only as its owner
owner=$(sed -n 's/.*NAME="OWNER".*VALUE="\([^"]*\)".*/\1/p' "$root/ghidra/Scrapland.rep/project.prp" 2> /dev/null)
[ -n "$owner" ] && export _JAVA_OPTIONS="${_JAVA_OPTIONS:+$_JAVA_OPTIONS }-Duser.name=$owner"
script=$1
shift
cd "$root" || exit 1
"$ghidra/support/analyzeHeadless" ghidra Scrapland -process Scrap.exe -noanalysis \
	-scriptPath tools/ghidra -postScript "$script" "$@" 2>&1 |
	grep -E '^(INFO|WARN|ERROR) .*(\(GhidraScript\)|Script|REPORT)|^(WARN|ERROR) [A-Za-z]+\.java>|^[A-Za-z]+Symbols:|Exception|error:' |
	grep -v 'Ghidra Script Analyzer'
exit "${PIPESTATUS[0]}"
