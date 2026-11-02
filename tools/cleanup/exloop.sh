#!/bin/bash
# build-linux64, add the missing extern declarations, until nothing changes
cd "$(dirname "$0")/../.." || exit 1
log=${EXLOG:-/tmp/exloop.log}
for i in $(seq 1 ${EXROUNDS:-8}); do
	cmake --build build-linux64 -j"$(nproc)" -- -k 2>&1 | sed "s|$PWD/||" > "$log"
	n=$(grep -c "error:" "$log")
	echo "round $i: $n errors"
	[ "$n" = 0 ] && break
	c=$(python3 tools/cleanup/addextern.py "$log" | tee /dev/stderr | wc -l)
	[ "$c" = 0 ] && break
done
grep -E "error:" "$log" | sort -u | head -30
