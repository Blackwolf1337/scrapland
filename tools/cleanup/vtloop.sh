#!/bin/bash
# build-linux64, fixsig and fixcall from its errors until nothing changes; then the errors left
cd "$(dirname "$0")/../.." || exit 1
log=${VTLOG:-/tmp/vtloop.log}
for i in 1 2 3 4 5 6; do
	cmake --build build-linux64 -j"$(nproc)" -- -k 2>&1 | sed "s|$PWD/||" > "$log"
	n=$(grep -c "error:" "$log")
	echo "round $i: $n errors"
	[ "$n" = 0 ] && break
	a=$(python3 tools/cleanup/vtgen.py fixsig "$log" | grep -vc '^report')
	b=$(python3 tools/cleanup/vtgen.py fixcall "$log" | grep -vc '^report')
	echo "  fixsig $a, fixcall $b"
	[ "$a" = 0 ] && [ "$b" = 0 ] && break
done
grep -E "error:" "$log" | sort -u | head -40
