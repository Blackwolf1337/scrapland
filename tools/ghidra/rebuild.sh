#!/bin/bash
# Builds a Ghidra project for Scrap.exe from scratch: import, auto analysis, the Python 1.5.2 headers, the engine
# headers of include/scrap (checked by CheckLayouts), the naming passes, then the names, signatures and comments from
# symbols/ (ImportSymbols after the passes, so names given by hand win), and last the header types in place of the
# placeholders of the analysis (ReplacePlaceholders).
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
	-postScript ImportHeaders.java tools/ghidra/python152/Include/Python.h -Itools/ghidra/include -Itools/ghidra/python152/config \
	-Itools/ghidra/python152/Include \
	-postScript ImportHeaders.java tools/ghidra/include/scrap_all.h -Itools/ghidra/include -Iinclude \
	-Itools/ghidra/python152/config -Itools/ghidra/python152/Include -DSD_GHIDRA -C__vectorcall=__thiscall \
	-postScript CheckLayouts.java include/scrap \
	-postScript NamePyBindings.java \
	-postScript NameVtables.java \
	-postScript NameFromStrings.java symbols/srcstrings.tsv \
	-postScript ImportSymbols.java symbols \
	-postScript ReplacePlaceholders.java tools/rename/names.tsv include/scrap tools/ghidra/python152/Include 2>&1 |
	grep -E '(Symbols|Bindings|Vtables|Strings|Headers|Layouts|Placeholders): |REPORT: (Analysis|Import|Save)|Exception|ERROR' |
	grep -v 'Invalid PNG data'
exit "${PIPESTATUS[0]}"
