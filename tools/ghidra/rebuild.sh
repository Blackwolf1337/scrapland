#!/bin/bash
# Builds a Ghidra project for Scrap.exe from scratch: import, auto analysis, the Python 1.5.2 headers, the naming
# passes, then the names,
# signatures and comments from symbols/ (ImportSymbols last, so names given by hand win over the passes).
#
# usage: tools/ghidra/rebuild.sh [project dir] [project name]   default ghidra Scrapland; the project must not exist
#        GHIDRA_DIR, GHIDRA_JAVA as in headless.sh
root=$(cd "$(dirname "$0")/../.." && pwd)
dir=${1:-$root/ghidra}
name=${2:-Scrapland}
ghidra=${GHIDRA_DIR:-/opt/ghidra}
export JAVA_HOME=${GHIDRA_JAVA:-$(find /usr/lib/jvm -maxdepth 1 -name 'java-*-openjdk' | sort -V | tail -1)}
if [ -e "$dir/$name.gpr" ]; then
	echo "rebuild.sh: $dir/$name.gpr exists, remove it first or pick another project" >&2
	exit 1
fi
mkdir -p "$dir"
cd "$root" || exit 1
"$ghidra/support/analyzeHeadless" "$dir" "$name" -import game/Bin/Scrap.exe -processor x86:LE:32:default \
	-cspec windows -analysisTimeoutPerFile 7200 -scriptPath tools/ghidra \
	-postScript ImportHeaders.java third_party/python-1.5.2/Include/Python.h -Itools/ghidra/include -Ithird_party/python-1.5.2/config \
	-Ithird_party/python-1.5.2/Include \
	-postScript NamePyBindings.java \
	-postScript NameVtables.java \
	-postScript NameFromStrings.java symbols/srcstrings.tsv \
	-postScript ImportSymbols.java symbols 2>&1 |
	grep -E '(Symbols|Bindings|Vtables|Strings|Headers): |REPORT: (Analysis|Import|Save)|Exception|ERROR' |
	grep -v 'Invalid PNG data'
exit "${PIPESTATUS[0]}"
