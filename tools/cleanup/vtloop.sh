#!/bin/bash
# build-linux64, then the first fixer that changes something, from its errors; until nothing changes. One fixer per
# build, because the fixers edit by the line numbers of the log (fixsig and protos insert lines).
cd "$(dirname "$0")/../.." || exit 1
log=${VTLOG:-/tmp/vtloop.log}
for i in $(seq 1 ${VTROUNDS:-30}); do
	cmake --build build-linux64 -j"$(nproc)" -- -k 2>&1 | sed "s|$PWD/||" > "$log"
	n=$(grep -c "error:" "$log")
	echo "round $i: $n errors"
	[ "$n" = 0 ] && break
	done_one=
	for t in fixsig fixcall fixmember fixvt protos; do
		c=$(python3 tools/cleanup/vtgen.py $t "$log" | grep -vc '^report')
		if [ "$c" != 0 ]; then echo "  $t $c"; done_one=1; break; fi
	done
	[ -z "$done_one" ] && break
done
grep -E "error:" "$log" | sort -u | head -40
